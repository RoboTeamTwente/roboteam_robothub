#include <basestation/Basestation.hpp>

#include <basestation/LibusbUtilities.h>

#include <iostream>

namespace rtt::robothub::basestation {

constexpr int BASESTATION_VENDOR_ID = 1155;
constexpr int BASESTATION_PRODUCT_ID = 22336;
constexpr int INTERFACE_NUMBER = 1;
constexpr unsigned char TRANSFER_IN_BUFFER_ENDPOINT = 0x01;
constexpr unsigned char TRANSFER_OUT_BUFFER_ENDPOINT = 0x81;
constexpr unsigned int TRANSFER_TIMEOUT_MS = 500;
constexpr int TRANSFER_IN_BUFFER_SIZE = 4096;
constexpr int INCOMING_MESSAGE_TIMEOUT_MS = 100;

Basestation::Basestation(const libusb_device* device) {
    int error;

    // TODO: Make use of error codes for more accurate debugging
    error = libusb_open(this->device, &this->device_handle);
    if (error)
        throw FailedToOpenDeviceException("Failed to open libusb device");

    // TODO: Figure out if this is necessary
    error = libusb_set_auto_detach_kernel_driver(this->device_handle, true);
    if (error) {
        libusb_close(this->device_handle);
        throw FailedToOpenDeviceException("Failed to set auto detach kernel driver");
    }

    error = libusb_claim_interface(this->device_handle, INTERFACE_NUMBER);
    if (error) {
        libusb_close(this->device_handle);
        throw FailedToOpenDeviceException("Failed to claim interface");
    }
}

Basestation::~Basestation() {
    libusb_close(this->device_handle);
}

bool Basestation::sendMessageToBasestation(const BasestationMessage& message) const {
    int bytesSent;
    int error = libusb_bulk_transfer(
        this->device_handle,
        TRANSFER_OUT_BUFFER_ENDPOINT,
        message.payload,
        message.payload_size,
        &bytesSent,
        TRANSFER_TIMEOUT_MS
    );

    if (error) { // TODO: Handle specific error codes for more accurate debugging
        std::cout << "Failed to send message to basestation" << std::endl;
        return false;
    }
    return true;
}

BasestationMessage Basestation::readIncomingMessage() const {
    BasestationMessage message = {
        .payload = nullptr,
        .payload_size = 0
    };

    uint8_t buffer[TRANSFER_IN_BUFFER_SIZE];
    int bytes_received = 0;
    int error = libusb_bulk_transfer(this->device_handle, TRANSFER_IN_BUFFER_ENDPOINT, buffer, TRANSFER_IN_BUFFER_SIZE, &bytes_received, INCOMING_MESSAGE_TIMEOUT_MS);
    
    if (error != LIBUSB_SUCCESS && error != LIBUSB_ERROR_TIMEOUT) {
        std::cout << "Error while reading message" << std::endl; // TODO: More accurate error handling
        return message;
    }

    if (bytes_received > 0) {
        message.payload = buffer;
        message.payload_size = bytes_received;
    }

    return message;
}

bool Basestation::equals(const Basestation& other) const {
    return this->address == other.address;
}
bool Basestation::equals(const libusb_device* other) const {
    return this->device == other;
    // TODO: Check if pointers to device match if they point to the same basestation
}

FailedToOpenDeviceException::FailedToOpenDeviceException(const std::string& message) : message(message) {}

const char* FailedToOpenDeviceException::what() const noexcept {
    return this->message.c_str();
}

} // namespace rtt::robothub::basestation