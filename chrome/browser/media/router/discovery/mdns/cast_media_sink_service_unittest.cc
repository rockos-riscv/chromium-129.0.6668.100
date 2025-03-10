// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"

#include <inttypes.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_test_helpers.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/test/mock_dns_sd_registry.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "components/media_router/common/providers/cast/channel/cast_device_capability.h"
#include "components/media_router/common/providers/cast/channel/cast_socket.h"
#include "components/media_router/common/providers/cast/channel/cast_socket_service.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "components/media_router/common/test/test_helper.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/ip_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using cast_channel::CastDeviceCapability;
using cast_channel::CastDeviceCapabilitySet;
using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

namespace {

net::IPEndPoint CreateIPEndPoint(int num) {
  net::IPAddress ip_address;
  CHECK(ip_address.AssignFromIPLiteral(
      base::StringPrintf("192.168.0.10%d", num)));
  return net::IPEndPoint(ip_address, 8009 + num);
}

media_router::DnsSdService CreateDnsService(
    int num,
    CastDeviceCapabilitySet capabilities) {
  net::IPEndPoint ip_endpoint = CreateIPEndPoint(num);
  media_router::DnsSdService service;
  service.service_name =
      "_myDevice." + std::string(media_router::kCastServiceType);
  service.ip_address = ip_endpoint.address().ToString();
  service.service_host_port = net::HostPortPair::FromIPEndPoint(ip_endpoint);
  service.service_data.push_back(base::StringPrintf("id=service %d", num));
  service.service_data.push_back(
      base::StringPrintf("fn=friendly name %d", num));
  service.service_data.push_back(base::StringPrintf("md=model name %d", num));
  service.service_data.push_back(
      base::StringPrintf("ca=%" PRIu64, capabilities.ToEnumBitmask()));

  return service;
}

}  // namespace

namespace media_router {

class CastMediaSinkServiceTest : public ::testing::Test {
 public:
  CastMediaSinkServiceTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        mock_cast_socket_service_(
            new cast_channel::MockCastSocketService(task_runner_)),
        media_sink_service_(new TestCastMediaSinkService(
            mock_cast_socket_service_.get(),
            DiscoveryNetworkMonitor::GetInstance())) {}

  CastMediaSinkServiceTest(CastMediaSinkServiceTest&) = delete;
  CastMediaSinkServiceTest& operator=(CastMediaSinkServiceTest&) = delete;

  void TearDown() override {
    media_sink_service_.reset();
    // Wait for `media_sink_service_.impl()` to be destroyed on its own sequence
    // runner.
    task_runner_->RunUntilIdle();
  }

  void SetUpTestDnsdRegistry(MockDnsSdRegistry* test_dns_sd_registry) {
    EXPECT_CALL(*test_dns_sd_registry, AddObserver(media_sink_service_.get()));
    EXPECT_CALL(*test_dns_sd_registry, RegisterDnsSdListener(_));
    media_sink_service_->SetDnsSdRegistryForTest(test_dns_sd_registry);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  std::unique_ptr<cast_channel::MockCastSocketService>
      mock_cast_socket_service_;
  std::unique_ptr<TestCastMediaSinkService> media_sink_service_;
};

TEST_F(CastMediaSinkServiceTest, DiscoverSinksNow) {
  MockDnsSdRegistry test_dns_sd_registry(media_sink_service_.get());
  SetUpTestDnsdRegistry(&test_dns_sd_registry);
  media_sink_service_->Initialize(base::DoNothing(), base::DoNothing(),
                                  nullptr);
  task_runner_->RunUntilIdle();

  EXPECT_CALL(test_dns_sd_registry, ResetAndDiscover());
  media_sink_service_->DiscoverSinksNow();

  EXPECT_CALL(test_dns_sd_registry, RemoveObserver(media_sink_service_.get()));
  EXPECT_CALL(test_dns_sd_registry, UnregisterDnsSdListener(_));
  media_sink_service_.reset();
  task_runner_->RunUntilIdle();
}

TEST_F(CastMediaSinkServiceTest, TestOnDnsSdEvent) {
  media_sink_service_->Initialize(base::DoNothing(), base::DoNothing(),
                                  nullptr);
  auto* mock_impl = media_sink_service_->mock_impl();
  ASSERT_TRUE(mock_impl);
  EXPECT_CALL(*mock_impl, DoStart()).WillOnce(InvokeWithoutArgs([this]() {
    EXPECT_TRUE(this->task_runner_->RunsTasksInCurrentSequence());
  }));
  task_runner_->RunUntilIdle();

  DnsSdService service1 = CreateDnsService(
      1, {CastDeviceCapability::kVideoOut, CastDeviceCapability::kAudioOut});
  DnsSdService service2 =
      CreateDnsService(2, {CastDeviceCapability::kMultizoneGroup});
  DnsSdService service3 = CreateDnsService(3, {});

  // Add dns services.
  DnsSdRegistry::DnsSdServiceList service_list{service1, service2, service3};

  // Invoke CastSocketService::OpenSocket on the IO thread.
  media_sink_service_->OnDnsSdEvent(kCastServiceType, service_list);

  std::vector<MediaSinkInternal> sinks;
  EXPECT_CALL(*mock_impl, OpenChannels(_, _)).WillOnce(SaveArg<0>(&sinks));

  // Invoke OpenChannels on |task_runner_|.
  task_runner_->RunUntilIdle();
  // Verify sink content
  ASSERT_EQ(3u, sinks.size());
  EXPECT_EQ(SinkIconType::CAST, sinks[0].sink().icon_type());
  EXPECT_EQ(SinkIconType::CAST_AUDIO_GROUP, sinks[1].sink().icon_type());
  EXPECT_EQ(SinkIconType::CAST_AUDIO, sinks[2].sink().icon_type());
}

TEST_F(CastMediaSinkServiceTest, DiscoveryDelayed) {
  // When the flag is enabled, initializing the CastMSS doesn't automatically
  // trigger discovery.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      media_router::kDelayMediaSinkDiscovery);

  EXPECT_CALL(*media_sink_service_, StartMdnsDiscovery).Times(0);
  media_sink_service_->Initialize(base::DoNothing(), base::DoNothing(),
                                  nullptr);
}

#if !BUILDFLAG(IS_WIN)
// TODO: crbug.com/345056325 - Remove this test after the
// kDelayMediaSinkDiscovery feature is enabled by default.
TEST_F(CastMediaSinkServiceTest, DiscoveryOnStartup) {
  // When the flag is disabled, initializing the CastMSS will automatically
  // trigger discovery.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      media_router::kDelayMediaSinkDiscovery);

  EXPECT_CALL(*media_sink_service_, StartMdnsDiscovery);
  media_sink_service_->Initialize(base::DoNothing(), base::DoNothing(),
                                  nullptr);
}
#endif

TEST_F(CastMediaSinkServiceTest, DiscoveryPermissionRejected) {
  base::MockCallback<base::RepeatingClosure> cb;
  MockDnsSdRegistry test_dns_sd_registry(media_sink_service_.get());
  SetUpTestDnsdRegistry(&test_dns_sd_registry);
  media_sink_service_->Initialize(base::DoNothing(), cb.Get(), nullptr);
  task_runner_->RunUntilIdle();

  EXPECT_CALL(cb, Run());
  test_dns_sd_registry.SimulatePermissionRejected();

  EXPECT_CALL(test_dns_sd_registry, RemoveObserver(media_sink_service_.get()));
  EXPECT_CALL(test_dns_sd_registry, UnregisterDnsSdListener(_));
  media_sink_service_.reset();
  task_runner_->RunUntilIdle();
}

}  // namespace media_router
