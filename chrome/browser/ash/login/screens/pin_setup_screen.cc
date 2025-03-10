// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/pin_setup_screen.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/pin_setup_screen_handler.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "ui/display/screen.h"

namespace ash {
namespace {

constexpr const char kUserActionDoneButtonClicked[] = "done-button";
constexpr const char kUserActionSkipButtonClickedOnStart[] =
    "skip-button-on-start";
constexpr const char kUserActionSkipButtonClickedInFlow[] =
    "skip-button-in-flow";

struct PinSetupUserAction {
  const char* name_;
  PinSetupScreen::UserAction uma_name_;
};

const PinSetupUserAction actions[] = {
    {kUserActionDoneButtonClicked,
     PinSetupScreen::UserAction::kDoneButtonClicked},
    {kUserActionSkipButtonClickedOnStart,
     PinSetupScreen::UserAction::kSkipButtonClickedOnStart},
    {kUserActionSkipButtonClickedInFlow,
     PinSetupScreen::UserAction::kSkipButtonClickedInFlow},
};

void RecordPinSetupScreenAction(PinSetupScreen::UserAction value) {
  base::UmaHistogramEnumeration("OOBE.PinSetupScreen.UserActions", value);
}

void RecordUserAction(const std::string& action_id) {
  for (const auto& el : actions) {
    if (action_id == el.name_) {
      RecordPinSetupScreenAction(el.uma_name_);
      return;
    }
  }
  NOTREACHED_IN_MIGRATION() << "Unexpected action id: " << action_id;
}

}  // namespace

// static
std::string PinSetupScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::DONE:
      return "Done";
    case Result::USER_SKIP:
      return "Skipped";
    case Result::TIMED_OUT:
      return "TimedOut";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

PinSetupScreen::PinSetupScreen(base::WeakPtr<PinSetupScreenView> view,
                               const ScreenExitCallback& exit_callback)
    : BaseScreen(PinSetupScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback),
      auth_performer_(UserDataAuthClient::Get()),
      cryptohome_pin_engine_(&auth_performer_) {
  DCHECK(view_);

  quick_unlock::PinBackend::GetInstance()->HasLoginSupport(base::BindOnce(
      &PinSetupScreen::OnHasLoginSupport, weak_ptr_factory_.GetWeakPtr()));
}

PinSetupScreen::~PinSetupScreen() = default;

bool PinSetupScreen::ShouldBeSkipped(const WizardContext& context) const {
  if (!context.extra_factors_token.has_value()) {
    return true;
  }
  if (!ash::AuthSessionStorage::Get()->IsValid(
          context.extra_factors_token.value())) {
    return true;
  }
  AccountId account_id = ash::AuthSessionStorage::Get()
                             ->Peek(context.extra_factors_token.value())
                             ->GetAccountId();
  if (context.skip_post_login_screens_for_tests ||
      cryptohome_pin_engine_.ShouldSkipSetupBecauseOfPolicy(account_id)) {
    return true;
  }

  // If cryptohome takes very long to respond, `has_login_support_` may be null
  // here, but this is very unusual.
  LOG_IF(WARNING, !has_login_support_.has_value())
      << "Could not determine hardware support support for login";
  // Show pin setup if we have hardware support for login with pin.
  if (has_login_support_.value_or(false)) {
    return false;
  }

  // Show the screen if the device is in tablet mode or tablet mode first user
  // run is forced on the device.
  if (display::Screen::GetScreen()->InTabletMode() ||
      switches::ShouldOobeUseTabletModeFirstRun()) {
    return false;
  }

  return true;
}

bool PinSetupScreen::MaybeSkip(WizardContext& context) {
  if (ShouldBeSkipped(context)) {
    ClearAuthData(context);
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  return false;
}

void PinSetupScreen::ShowImpl() {
  token_lifetime_timeout_.Start(FROM_HERE,
                                quick_unlock::AuthToken::kTokenExpiration,
                                base::BindOnce(&PinSetupScreen::OnTokenTimedOut,
                                               weak_ptr_factory_.GetWeakPtr()));
  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  quick_unlock_storage->MarkStrongAuth();
  std::string token;
  CHECK(context()->extra_factors_token);
  token = *context()->extra_factors_token;
  bool is_child_account =
      user_manager::UserManager::Get()->IsLoggedInAsChildUser();

  if (view_) {
    view_->Show(token, is_child_account, has_login_support_.value_or(false));
  }
}

void PinSetupScreen::HideImpl() {
  token_lifetime_timeout_.Stop();
  ClearAuthData(*context());
}

void PinSetupScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionDoneButtonClicked) {
    RecordUserAction(action_id);
    token_lifetime_timeout_.Stop();
    exit_callback_.Run(Result::DONE);
    return;
  }
  if (action_id == kUserActionSkipButtonClickedOnStart ||
      action_id == kUserActionSkipButtonClickedInFlow) {
    RecordUserAction(action_id);
    token_lifetime_timeout_.Stop();
    exit_callback_.Run(Result::USER_SKIP);
    return;
  }
  BaseScreen::OnUserAction(args);
}

void PinSetupScreen::ClearAuthData(WizardContext& context) {
  if (context.extra_factors_token.has_value()) {
    ash::AuthSessionStorage::Get()->Invalidate(
        context.extra_factors_token.value(), base::DoNothing());
    context.extra_factors_token = std::nullopt;
  }
}

void PinSetupScreen::OnHasLoginSupport(bool login_available) {
  has_login_support_ = login_available;
  if (!is_hidden() && view_) {
    view_->SetLoginSupportAvailable(has_login_support_.value());
  }
}

void PinSetupScreen::OnTokenTimedOut() {
  ClearAuthData(*context());
  exit_callback_.Run(Result::TIMED_OUT);
}

}  // namespace ash
