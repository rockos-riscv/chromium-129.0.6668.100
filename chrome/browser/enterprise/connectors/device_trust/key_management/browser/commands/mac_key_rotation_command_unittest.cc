// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/mac_key_rotation_command.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/device_trust_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/common/key_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/mock_secure_enclave_client.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mock_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/metrics_util.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/client_certificates/core/cloud_management_delegate.h"
#include "components/enterprise/client_certificates/core/mock_cloud_management_delegate.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;
using testing::_;
using testing::InSequence;
using testing::Invoke;
using testing::Return;

namespace enterprise_connectors {

using test::MockKeyNetworkDelegate;
using test::MockKeyPersistenceDelegate;
using test::MockSecureEnclaveClient;
using test::ScopedKeyPersistenceDelegateFactory;
using HttpResponseCode =
    enterprise_connectors::test::MockKeyNetworkDelegate::HttpResponseCode;

namespace {

// Add a couple of seconds to the exact timeout time.
const base::TimeDelta kTimeoutTime =
    timeouts::kHandshakeTimeout + base::Seconds(2);

constexpr char kNonce[] = "nonce";
constexpr char kFakeDMToken[] = "fake-browser-dm-token";
constexpr char kFakeDmServerUrl[] =
    "https://m.google.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";

constexpr HttpResponseCode kSuccessCode = 200;
constexpr HttpResponseCode kFailureCode = 400;
constexpr HttpResponseCode kKeyConflictCode = 409;

}  // namespace

class MacKeyRotationCommandTest : public testing::Test,
                                  public testing::WithParamInterface<bool> {
 protected:
  MacKeyRotationCommandTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    feature_list_.InitWithFeatureState(
        enterprise_connectors::kDTCKeyRotationUploadedBySharedAPIEnabled,
        is_key_uploaded_by_shared_api());
  }

  void SetUp() override {
    auto mock_secure_enclave_client =
        std::make_unique<MockSecureEnclaveClient>();
    mock_secure_enclave_client_ = mock_secure_enclave_client.get();
    SecureEnclaveClient::SetInstanceForTesting(
        std::move(mock_secure_enclave_client));

    params_.nonce = kNonce;

    auto mock_persistence_delegate = scoped_factory_.CreateMockedECDelegate();
    mock_persistence_delegate_ = mock_persistence_delegate.get();

    if (is_key_uploaded_by_shared_api()) {
      auto mock_cloud_delegate = std::make_unique<
          enterprise_attestation::MockCloudManagementDelegate>();
      mock_cloud_delegate_ = mock_cloud_delegate.get();

      KeyRotationManager::SetForTesting(KeyRotationManager::CreateForTesting(
          std::move(mock_cloud_delegate),
          std::move(mock_persistence_delegate)));

      rotation_command_ = base::WrapUnique(new MacKeyRotationCommand(
          test_shared_loader_factory_, &fake_dm_token_storage_,
          &device_management_service_));
    } else {
      params_.dm_token = kFakeDMToken;
      params_.dm_server_url = kFakeDmServerUrl;

      auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
      mock_network_delegate_ = mock_network_delegate.get();

      KeyRotationManager::SetForTesting(KeyRotationManager::CreateForTesting(
          std::move(mock_network_delegate),
          std::move(mock_persistence_delegate)));

      rotation_command_ = base::WrapUnique(
          new MacKeyRotationCommand(test_shared_loader_factory_));
    }
  }

  void FastForwardBeyondTimeout() {
    task_environment_.FastForwardBy(kTimeoutTime + base::Seconds(2));
    task_environment_.RunUntilIdle();
  }

  void SetUpDmToken(std::string dm_token = kFakeDMToken) {
    if (is_key_uploaded_by_shared_api()) {
      EXPECT_CALL(*mock_cloud_delegate_, GetDMToken())
          .WillRepeatedly(Return(dm_token));
    }
  }

  void UploadPublicKey(HttpResponseCode response_code) {
    if (is_key_uploaded_by_shared_api()) {
      policy::DMServerJobResult result;
      result.response_code = response_code;

      EXPECT_CALL(*mock_cloud_delegate_, UploadBrowserPublicKey(_, _))
          .WillOnce(RunOnceCallback<1>(result));
    } else {
      EXPECT_CALL(
          *mock_network_delegate_,
          SendPublicKeyToDmServer(GURL(kFakeDmServerUrl), kFakeDMToken, _, _))
          .WillOnce(RunOnceCallback<3>(response_code));
    }
  }

  bool is_key_uploaded_by_shared_api() { return GetParam(); }

  base::test::ScopedFeatureList feature_list_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<MacKeyRotationCommand> rotation_command_;
  raw_ptr<MockSecureEnclaveClient, DanglingUntriaged>
      mock_secure_enclave_client_ = nullptr;
  raw_ptr<MockKeyNetworkDelegate, DanglingUntriaged> mock_network_delegate_ =
      nullptr;
  raw_ptr<enterprise_attestation::MockCloudManagementDelegate,
          DanglingUntriaged>
      mock_cloud_delegate_ = nullptr;
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
  testing::StrictMock<policy::MockJobCreationHandler> job_creation_handler_;
  policy::FakeDeviceManagementService device_management_service_{
      &job_creation_handler_};

  raw_ptr<MockKeyPersistenceDelegate, DanglingUntriaged>
      mock_persistence_delegate_ = nullptr;
  test::ScopedKeyPersistenceDelegateFactory scoped_factory_;
  KeyRotationCommand::Params params_;
};

// Tests a failed key rotation due to the secure enclave not being supported.
TEST_P(MacKeyRotationCommandTest, RotateFailure_SecureEnclaveUnsupported) {
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(false));

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED_OS_RESTRICTION, future.Get());
}

// Tests a failed key rotation due to failure creating a new signing key pair.
TEST_P(MacKeyRotationCommandTest, RotateFailure_CreateKeyFailure) {
  EXPECT_CALL(*mock_persistence_delegate_,
              LoadKeyPair(KeyStorageType::kPermanent, _));
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair())
      .WillOnce(Invoke([]() { return nullptr; }));
  SetUpDmToken();

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED, future.Get());
}

// Tests a failed key rotation due to a store key failure.
TEST_P(MacKeyRotationCommandTest, RotateFailure_StoreKeyFailure) {
  EXPECT_CALL(*mock_persistence_delegate_,
              LoadKeyPair(KeyStorageType::kPermanent, _));
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(false));
  SetUpDmToken();

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED, future.Get());
}

// Tests a failed key rotation when uploading a the key to the dm server
// fails due to a key conflict failure.
TEST_P(MacKeyRotationCommandTest, RotateFailure_KeyConflict) {
  InSequence s;
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_,
              LoadKeyPair(KeyStorageType::kPermanent, _));
  SetUpDmToken();
  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));
  UploadPublicKey(kKeyConflictCode);
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED_KEY_CONFLICT, future.Get());
}

// Tests a failed key rotation due to a failure sending the key to the dm
// server.
TEST_P(MacKeyRotationCommandTest, RotateFailure_UploadKeyFailure) {
  InSequence s;
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_,
              LoadKeyPair(KeyStorageType::kPermanent, _));
  SetUpDmToken();
  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));
  UploadPublicKey(kFailureCode);
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED, future.Get());
}

// Tests when the browser has invalid permissions.
TEST_P(MacKeyRotationCommandTest, Rotate_InvalidPermissions) {
  SetUpDmToken();
  EXPECT_CALL(*mock_persistence_delegate_,
              LoadKeyPair(KeyStorageType::kPermanent, _));
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(false));

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED_INVALID_PERMISSIONS,
            future.Get());
}

// Tests when the key rotation is successful.
TEST_P(MacKeyRotationCommandTest, Rotate_Success) {
  EXPECT_CALL(*mock_persistence_delegate_,
              LoadKeyPair(KeyStorageType::kPermanent, _));
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));
  SetUpDmToken();
  UploadPublicKey(kSuccessCode);

  EXPECT_CALL(*mock_persistence_delegate_, CleanupTemporaryKeyData());

  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::SUCCEEDED, future.Get());

  // Advancing beyond timeout time doesn't cause any crashes.
  FastForwardBeyondTimeout();
}

// Tests what happens when the key rotation succeeds beyond the timeout limit
// before the command object is destroyed.
TEST_P(MacKeyRotationCommandTest, Rotate_Timeout_ReturnBeforeDestruction) {
  EXPECT_CALL(*mock_persistence_delegate_,
              LoadKeyPair(KeyStorageType::kPermanent, _));
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));
  SetUpDmToken();

  if (is_key_uploaded_by_shared_api()) {
    base::OnceCallback<void(policy::DMServerJobResult)> captured_callback;
    EXPECT_CALL(*mock_cloud_delegate_, UploadBrowserPublicKey(_, _))
        .WillOnce(Invoke(
            [&captured_callback](
                const enterprise_management::DeviceManagementRequest& request,
                base::OnceCallback<void(policy::DMServerJobResult)> callback) {
              captured_callback = std::move(callback);
            }));

    base::test::TestFuture<KeyRotationCommand::Status> future;
    rotation_command_->Trigger(params_, future.GetCallback());

    FastForwardBeyondTimeout();

    EXPECT_EQ(KeyRotationCommand::Status::TIMED_OUT, future.Get());

    policy::DMServerJobResult result;
    result.response_code = kSuccessCode;
    std::move(captured_callback).Run(result);
  } else {
    base::OnceCallback<void(int)> captured_callback;
    EXPECT_CALL(
        *mock_network_delegate_,
        SendPublicKeyToDmServer(GURL(kFakeDmServerUrl), kFakeDMToken, _, _))
        .WillOnce(Invoke(
            [&captured_callback](const GURL& url, const std::string& dm_token,
                                 const std::string& body,
                                 base::OnceCallback<void(int)> callback) {
              captured_callback = std::move(callback);
            }));

    base::test::TestFuture<KeyRotationCommand::Status> future;
    rotation_command_->Trigger(params_, future.GetCallback());

    FastForwardBeyondTimeout();

    EXPECT_EQ(KeyRotationCommand::Status::TIMED_OUT, future.Get());

    // Invoking the callback shouldn't crash.
    std::move(captured_callback).Run(kSuccessCode);
  }

  // Make sure the callback runs before exiting the test.
  task_environment_.RunUntilIdle();
}

// Tests what happens when the key rotation succeeds beyond the timeout limit
// after the command object is destroyed.
TEST_P(MacKeyRotationCommandTest, Rotate_Timeout_ReturnAfterDestruction) {
  EXPECT_CALL(*mock_persistence_delegate_,
              LoadKeyPair(KeyStorageType::kPermanent, _));
  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair());
  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));
  SetUpDmToken();

  if (is_key_uploaded_by_shared_api()) {
    base::OnceCallback<void(policy::DMServerJobResult)> captured_callback;
    EXPECT_CALL(*mock_cloud_delegate_, UploadBrowserPublicKey(_, _))
        .WillOnce(Invoke(
            [&captured_callback](
                const enterprise_management::DeviceManagementRequest& request,
                base::OnceCallback<void(policy::DMServerJobResult)> callback) {
              captured_callback = std::move(callback);
            }));

    base::test::TestFuture<KeyRotationCommand::Status> future;
    rotation_command_->Trigger(params_, future.GetCallback());

    FastForwardBeyondTimeout();

    EXPECT_EQ(KeyRotationCommand::Status::TIMED_OUT, future.Get());

    rotation_command_.reset();

    policy::DMServerJobResult result;
    result.response_code = kSuccessCode;
    std::move(captured_callback).Run(result);
  } else {
    base::OnceCallback<void(int)> captured_callback;
    EXPECT_CALL(
        *mock_network_delegate_,
        SendPublicKeyToDmServer(GURL(kFakeDmServerUrl), kFakeDMToken, _, _))
        .WillOnce(Invoke(
            [&captured_callback](const GURL& url, const std::string& dm_token,
                                 const std::string& body,
                                 base::OnceCallback<void(int)> callback) {
              captured_callback = std::move(callback);
            }));

    base::test::TestFuture<KeyRotationCommand::Status> future;
    rotation_command_->Trigger(params_, future.GetCallback());

    FastForwardBeyondTimeout();

    EXPECT_EQ(KeyRotationCommand::Status::TIMED_OUT, future.Get());

    rotation_command_.reset();

    // Invoking the callback shouldn't crash because it is bound to a weak
    // pointer.
    std::move(captured_callback).Run(kSuccessCode);
  }

  // Make sure the callback runs before exiting the test.
  task_environment_.RunUntilIdle();
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Tests a failed key rotation due to an invalid command to rotate. Wrapping the
// test in a branding buildflag as it depends on the current channel being
// mocked as Stable, which only happens when branded.
TEST_P(MacKeyRotationCommandTest, RotateFailure_InvalidCommand) {
  static constexpr char kInvalidDmServerUrl[] =
      "https://example.com/"
      "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
      "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
      "7C1.2.3&request=browser_public_key_upload";

  EXPECT_CALL(*mock_secure_enclave_client_, VerifySecureEnclaveSupported())
      .WillOnce(Return(true));

  params_.dm_server_url = kInvalidDmServerUrl;
  base::test::TestFuture<KeyRotationCommand::Status> future;
  rotation_command_->Trigger(params_, future.GetCallback());
  EXPECT_EQ(KeyRotationCommand::Status::FAILED, future.Get());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

INSTANTIATE_TEST_SUITE_P(, MacKeyRotationCommandTest, testing::Bool());

}  // namespace enterprise_connectors
