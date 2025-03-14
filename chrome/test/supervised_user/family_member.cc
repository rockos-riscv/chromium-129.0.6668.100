// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/family_member.h"

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/supervised_user_capabilities.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "google_apis/gaia/core_account_id.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace supervised_user {

namespace {
CoreAccountId GetAccountId(Profile* profile) {
  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);
  CHECK(supervised_user_service) << "Incognito mode is not supported.";

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  CHECK(supervised_user::IsPrimaryAccountSubjectToParentalControls(
            identity_manager) == signin::Tribool::kTrue)
      << "Blocklist control page is only available to user who have that "
         "feature enabled. Check if member is a subject to parental controls.";

  return identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
}

std::string GetFamilyMemberSettingsUrlBase(const FamilyMember& member) {
  return base::StrCat({"https://families.google.com/u/0/manage/family/child/",
                       GetAccountId(member.browser()->profile()).ToString()});
}

GURL GetControlListUrlFor(const FamilyMember& member, std::string_view page) {
  return GURL(base::StrCat(
      {GetFamilyMemberSettingsUrlBase(member), "/exceptions/", page}));
}
}  // namespace

FamilyMember::FamilyMember(
    signin::test::TestAccount account,
    Browser& browser,
    const base::RepeatingCallback<bool(int, const GURL&, ui::PageTransition)>
        add_tab_function)
    : account_(account),
      browser_(browser),
      sign_in_functions_(base::BindLambdaForTesting(
                             [&browser]() -> Browser* { return &browser; }),
                         add_tab_function) {}
FamilyMember::~FamilyMember() = default;

GURL FamilyMember::GetBlockListUrlFor(const FamilyMember& member) const {
  return GetControlListUrlFor(member, "blocked");
}

GURL FamilyMember::GetAllowListUrlFor(const FamilyMember& member) const {
  return GetControlListUrlFor(member, "allowed");
}

GURL FamilyMember::GetPermissionsUrlFor(const FamilyMember& member) const {
  return GURL(
      base::StrCat({GetFamilyMemberSettingsUrlBase(member), "/permissions"}));
}

SupervisedUserService* FamilyMember::supervised_user_service() const {
  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(browser()->profile());
  CHECK(supervised_user_service) << "Incognito mode is not supported.";
  return supervised_user_service;
}

void FamilyMember::TurnOnSync() {
  sign_in_functions_.TurnOnSync(account_, 0);
}

CoreAccountId FamilyMember::GetAccountId() const {
  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(browser()->profile());
  CHECK(supervised_user_service) << "Incognito mode is not supported.";

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());

  CHECK(supervised_user::IsPrimaryAccountSubjectToParentalControls(
            identity_manager) == signin::Tribool::kTrue)
      << "Blocklist control page is only available to user who have that "
         "feature enabled. Check if member is a subject to parental controls. "
         "Account: "
      << account_.user;

  return identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
}

}  // namespace supervised_user
