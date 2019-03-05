/* Copyright 2019-present Barefoot Networks, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef STRATUM_HAL_LIB_BMV2_BMV2_CHASSIS_MANAGER_H_
#define STRATUM_HAL_LIB_BMV2_BMV2_CHASSIS_MANAGER_H_

#include "bm/bm_sim/dev_mgr.h"

#include <map>
#include <memory>
#include <utility>

#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/glue/integral_types.h"
#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"

namespace bm {
namespace sswitch {
class SimpleSwitchRunner;
}  // namespace sswitch
}  // namespace bm

namespace stratum {
namespace hal {
namespace bmv2 {

// Lock which protects chassis state across the entire switch.
extern absl::Mutex chassis_lock;

class Bmv2ChassisManager {
 public:
  virtual ~Bmv2ChassisManager();

  virtual ::util::Status PushChassisConfig(const ChassisConfig& config)
      EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);

  virtual ::util::Status VerifyChassisConfig(const ChassisConfig& config)
      SHARED_LOCKS_REQUIRED(chassis_lock);

  virtual ::util::Status RegisterEventNotifyWriter(
      const std::shared_ptr<WriterInterface<GnmiEventPtr>>& writer)
      LOCKS_EXCLUDED(gnmi_event_lock_);

  virtual ::util::Status UnregisterEventNotifyWriter()
      LOCKS_EXCLUDED(gnmi_event_lock_);

  virtual ::util::StatusOr<DataResponse> GetPortData(
      const DataRequest::Request& request)
      SHARED_LOCKS_REQUIRED(chassis_lock);

  virtual ::util::StatusOr<PortState> GetPortState(
      uint64 node_id, uint32 port_id)
      SHARED_LOCKS_REQUIRED(chassis_lock);

  virtual ::util::Status GetPortCounters(
      uint64 node_id, uint32 port_id, PortCounters* counters)
      SHARED_LOCKS_REQUIRED(chassis_lock);

  // Factory function for creating the instance of the class.
  static std::unique_ptr<Bmv2ChassisManager> CreateInstance(
      PhalInterface* phal_interface,
      std::map<uint64, ::bm::sswitch::SimpleSwitchRunner*>
        node_id_to_bmv2_runner);

  // Bmv2ChassisManager is neither copyable nor movable.
  Bmv2ChassisManager(const Bmv2ChassisManager&) = delete;
  Bmv2ChassisManager& operator=(const Bmv2ChassisManager&) = delete;
  Bmv2ChassisManager(Bmv2ChassisManager&&) = delete;
  Bmv2ChassisManager& operator=(Bmv2ChassisManager&&) = delete;

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  Bmv2ChassisManager(
      PhalInterface* phal_interface,
      std::map<uint64, ::bm::sswitch::SimpleSwitchRunner*>
        node_id_to_bmv2_runner);

  ::util::Status RegisterEventWriters()
        EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);
  ::util::Status UnregisterEventWriters()
        EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);

  // Forward PortStatus changed events through the appropriate node's registered
  // ChannelWriter<GnmiEventPtr> object.
  void SendPortOperStateGnmiEvent(uint64 node_id, uint64 port_id,
                                  PortState new_state)
      LOCKS_EXCLUDED(gnmi_event_lock_);

  friend ::util::Status PortStatusChangeCb(Bmv2ChassisManager* chassis_mgr,
                                           uint64 node_id,
                                           uint64 port_id,
                                           PortState new_state)
      LOCKS_EXCLUDED(chassis_lock);

  ::util::StatusOr<const SingletonPort*> GetSingletonPort(
       uint64 node_id, uint64 port_id) const
        SHARED_LOCKS_REQUIRED(chassis_lock);

  bool initialized_ GUARDED_BY(chassis_lock);

  // WriterInterface<GnmiEventPtr> object for sending event notifications.
  mutable absl::Mutex gnmi_event_lock_;
  std::shared_ptr<WriterInterface<GnmiEventPtr>> gnmi_event_writer_
      GUARDED_BY(gnmi_event_lock_);

  // Pointer to a PhalInterface implementation.
  PhalInterface* phal_interface_;  // not owned by this class.

  std::map<uint64, ::bm::sswitch::SimpleSwitchRunner*> node_id_to_bmv2_runner_;

  std::map<uint64, ::bm::PortMonitorIface::PortStatusCb>
      node_id_to_bmv2_port_status_cb_ GUARDED_BY(chassis_lock);

  // Map from node ID to another map from port ID to PortState representing
  // the state of the singleton port uniquely identified by (node ID, port ID).
  std::map<uint64, std::map<uint32, PortState>>
      node_id_to_port_id_to_port_state_ GUARDED_BY(chassis_lock);

  // Map from node ID to another map from port ID to SignletonPort representing
  // the config of the singleton port uniquely identified by (node ID, port ID).
  std::map<uint64, std::map<uint32, SingletonPort>>
      node_id_to_port_id_to_port_config_ GUARDED_BY(chassis_lock);
};

}  // namespace bmv2
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BMV2_BMV2_CHASSIS_MANAGER_H_