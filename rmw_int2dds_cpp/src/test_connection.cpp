// Copyright 2024 Int2DDS Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// Test program to verify int2dds-ffi connectivity and SPDP transmission

#include <iostream>
#include <thread>
#include <chrono>
#include "int2dds-ffi.h"  // NOLINT(build/include_subdir): vendored FFI header

int main()
{
  std::cout << "Testing int2dds-ffi with Participant creation..." << std::endl;
  std::cout << "This will test SPDP (Simple Participant Discovery Protocol) transmission.\n" <<
    std::endl;

    // Get DomainParticipantFactory
  Int2DdsParticipantFactory * factory = nullptr;
  Int2DdsRet ret = int2dds_domain_participant_factory_get_instance(&factory);
  if (ret != INT2DDS_RET_OK) {
    std::cerr << "Failed to get participant factory: " << ret << std::endl;
    return -1;
  }
  std::cout << "[OK] int2dds_domain_participant_factory_get_instance() succeeded" << std::endl;

    // Create DomainParticipant
  Int2DdsParticipant * participant = nullptr;
  int32_t domain_id = 0;

  const char * participant_name = "test_participant";
  ret = int2dds_create_participant(factory, participant_name, domain_id, &participant);
  if (ret != INT2DDS_RET_OK) {
    std::cerr << "Failed to create participant: " << ret << std::endl;
    int2dds_domain_participant_factory_finalize(factory);
    return -1;
  }
  std::cout << "[OK] int2dds_create_participant() succeeded" << std::endl;
  std::cout << "  - Participant name: " << participant_name << std::endl;
  std::cout << "  - Domain ID: " << domain_id << std::endl;

    // Create Publisher and Subscriber
  Int2DdsPublisher * publisher = nullptr;
  ret = int2dds_create_publisher(participant, &publisher);
  if (ret != INT2DDS_RET_OK) {
    std::cerr << "Failed to create publisher: " << ret << std::endl;
  } else {
    std::cout << "[OK] int2dds_create_publisher() succeeded" << std::endl;
  }

  Int2DdsSubscriber * subscriber = nullptr;
  ret = int2dds_create_subscriber(participant, &subscriber);
  if (ret != INT2DDS_RET_OK) {
    std::cerr << "Failed to create subscriber: " << ret << std::endl;
  } else {
    std::cout << "[OK] int2dds_create_subscriber() succeeded" << std::endl;
  }

    // Wait to allow SPDP to be sent
  std::cout << "\n[SPDP Test] Participant created. SPDP should be transmitted now." << std::endl;
  std::cout << "[SPDP Test] Waiting 5 seconds to allow periodic SPDP announcements..." << std::endl;
  std::cout << "[SPDP Test] You can use Wireshark to capture packets on UDP ports 7400-7411" <<
    std::endl;

  for (int i = 5; i > 0; i--) {
    std::cout << "  " << i << "..." << std::flush;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  std::cout << " Done!" << std::endl;

    // Cleanup
  if (subscriber != nullptr) {
    ret = int2dds_delete_subscriber(subscriber);
    if (ret == INT2DDS_RET_OK) {
      std::cout << "[OK] int2dds_delete_subscriber() succeeded" << std::endl;
    }
  }

  if (publisher != nullptr) {
    ret = int2dds_delete_publisher(publisher);
    if (ret == INT2DDS_RET_OK) {
      std::cout << "[OK] int2dds_delete_publisher() succeeded" << std::endl;
    }
  }

    // Delete Participant
  ret = int2dds_delete_participant(participant);
  if (ret != INT2DDS_RET_OK) {
    std::cerr << "Failed to delete participant: " << ret << std::endl;
    int2dds_domain_participant_factory_finalize(factory);
    return -1;
  }
  std::cout << "[OK] int2dds_delete_participant() succeeded" << std::endl;

    // Finalize factory
  ret = int2dds_domain_participant_factory_finalize(factory);
  if (ret != INT2DDS_RET_OK) {
    std::cerr << "Failed to finalize factory: " << ret << std::endl;
    return -1;
  }
  std::cout << "[OK] int2dds_domain_participant_factory_finalize() succeeded" << std::endl;

  std::cout << "\n========================================" << std::endl;
  std::cout << "Test PASSED!" << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << "\nParticipant was created and should have sent SPDP messages." << std::endl;
  std::cout << "Check Wireshark capture to verify RTPS packets were transmitted." << std::endl;

  return 0;
}
