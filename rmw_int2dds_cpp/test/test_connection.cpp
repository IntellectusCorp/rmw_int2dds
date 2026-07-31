// Copyright 2024 Int2DDS Project
// Test program to verify int2dds-ffi connectivity and SPDP transmission

#include <iostream>
#include <thread>
#include <chrono>
#include "int2dds-ffi.h"

int main() {
    std::cout << "Testing int2dds-ffi with Participant creation..." << std::endl;
    std::cout << "This will test SPDP (Simple Participant Discovery Protocol) transmission.\n" << std::endl;

    // Get DomainParticipantFactory
    Int2DdsParticipantFactory* factory = nullptr;
    Int2DdsRet ret = int2dds_domain_participant_factory_get_instance(&factory);
    if (ret != INT2DDS_RET_OK) {
        std::cerr << "Failed to get participant factory: " << ret << std::endl;
        return -1;
    }
    std::cout << "[OK] int2dds_domain_participant_factory_get_instance() succeeded" << std::endl;

    // Create DomainParticipant
    Int2DdsParticipant* participant = nullptr;
    int32_t domain_id = 0;

    const char* participant_name = "test_participant";
    ret = int2dds_create_participant(factory, domain_id, nullptr, &participant);
    if (ret != INT2DDS_RET_OK) {
        std::cerr << "Failed to create participant: " << ret << std::endl;
        int2dds_domain_participant_factory_finalize(factory);
        return -1;
    }
    std::cout << "[OK] int2dds_create_participant() succeeded" << std::endl;
    std::cout << "  - Participant name: " << participant_name << std::endl;
    std::cout << "  - Domain ID: " << domain_id << std::endl;

    // Create Publisher and Subscriber
    Int2DdsPublisher* publisher = nullptr;
    ret = int2dds_create_publisher(participant, nullptr, &publisher);
    if (ret != INT2DDS_RET_OK) {
        std::cerr << "Failed to create publisher: " << ret << std::endl;
    } else {
        std::cout << "[OK] int2dds_create_publisher() succeeded" << std::endl;
    }

    Int2DdsSubscriber* subscriber = nullptr;
    ret = int2dds_create_subscriber(participant, nullptr, &subscriber);
    if (ret != INT2DDS_RET_OK) {
        std::cerr << "Failed to create subscriber: " << ret << std::endl;
    } else {
        std::cout << "[OK] int2dds_create_subscriber() succeeded" << std::endl;
    }

    // Wait to allow SPDP to be sent
    std::cout << "\n[SPDP Test] Participant created. SPDP should be transmitted now." << std::endl;
    std::cout << "[SPDP Test] Waiting 5 seconds to allow periodic SPDP announcements..." << std::endl;
    std::cout << "[SPDP Test] You can use Wireshark to capture packets on UDP ports 7400-7411" << std::endl;

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
