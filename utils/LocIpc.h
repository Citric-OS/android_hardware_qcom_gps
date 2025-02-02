/* Copyright (c) 2017-2018, 2020-2021 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation, nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
Changes from Qualcomm Innovation Center are provided under the following license:

Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted (subject to the limitations in the
disclaimer below) provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __LOC_IPC__
#define __LOC_IPC__

#include <string>
#include <memory>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <mutex>
#include <LocThread.h>

using namespace std;
#include <unordered_set>

namespace loc_util {

class LocIpcRecver;
class LocIpcSender;

class ILocIpcListener {
protected:
    inline virtual ~ILocIpcListener() {}
public:
    // LocIpc client can overwrite this function to get notification
    // when the socket for LocIpc is ready to receive messages.
    inline virtual void onListenerReady() {}
    virtual void onReceive(const char* data, uint32_t len, const LocIpcRecver* recver) = 0;
};

class LocIpcQrtrWatcher {
    const unordered_set<int> mServicesToWatch;
    unordered_set<int> mClientsToWatch;
    mutex mMutex;
    inline bool isInWatch(const unordered_set<int>& idsToWatch, int id) {
        return idsToWatch.find(id) != idsToWatch.end();
    }
protected:
    inline virtual ~LocIpcQrtrWatcher() {}
    inline LocIpcQrtrWatcher(unordered_set<int> servicesToWatch)
            : mServicesToWatch(servicesToWatch) {}
public:
    enum class ServiceStatus { UP, DOWN };
    inline bool isServiceInWatch(int serviceId) {
        return isInWatch(mServicesToWatch, serviceId);
    }
    inline bool isClientInWatch(int nodeId) {
        lock_guard<mutex> lock(mMutex);
        return isInWatch(mClientsToWatch, nodeId);
    }
    inline void addClientToWatch(int nodeId) {
        lock_guard<mutex> lock(mMutex);
        mClientsToWatch.emplace(nodeId);
    }
    virtual void onServiceStatusChange(int sericeId, int instanceId, ServiceStatus status,
                                       uint32_t nodeId, uint32_t port) = 0;
    inline virtual void onClientGone(int nodeId, int portId) {}
    inline const unordered_set<int>& getServicesToWatch() { return mServicesToWatch; }
};

class LocIpc {
public:
    inline LocIpc() = default;
    inline virtual ~LocIpc() {
        stopNonBlockingListening();
    }

    static shared_ptr<LocIpcSender>
            getLocIpcLocalSender(const char* localSockName);
    static shared_ptr<LocIpcSender>
            getLocIpcInetUdpSender(const char* serverName, int32_t port);
    static shared_ptr<LocIpcSender>
            getLocIpcInetTcpSender(const char* serverName, int32_t port);
    static shared_ptr<LocIpcSender>
            getLocIpcQrtrSender(int service, int instance);

    static unique_ptr<LocIpcRecver>
            getLocIpcLocalRecver(const shared_ptr<ILocIpcListener>& listener,
                                 const char* localSockName);
    static unique_ptr<LocIpcRecver>
            getLocIpcInetUdpRecver(const shared_ptr<ILocIpcListener>& listener,
                                 const char* serverName, int32_t port);
    static unique_ptr<LocIpcRecver>
            getLocIpcInetTcpRecver(const shared_ptr<ILocIpcListener>& listener,
                                   const char* serverName, int32_t port);
    inline static unique_ptr<LocIpcRecver>
            getLocIpcQrtrRecver(const shared_ptr<ILocIpcListener>& listener,
                                int service, int instance) {
        const shared_ptr<LocIpcQrtrWatcher> qrtrWatcher = nullptr;
        return getLocIpcQrtrRecver(listener, service, instance, qrtrWatcher);
    }
    static unique_ptr<LocIpcRecver>
            getLocIpcQrtrRecver(const shared_ptr<ILocIpcListener>& listener,
                                int service, int instance,
                                const shared_ptr<LocIpcQrtrWatcher>& qrtrWatcher);

    static pair<shared_ptr<LocIpcSender>, unique_ptr<LocIpcRecver>>
            getLocIpcQmiLocServiceSenderRecverPair(const shared_ptr<ILocIpcListener>& listener,
                                                   int instance);

    // Listen for new messages in current thread. Calling this funciton will
    // block current thread.
    // The listening can be stopped by calling stopBlockingListening() passing
    // in the same ipcRecver obj handle.
    static bool startBlockingListening(LocIpcRecver& ipcRecver);
    static void stopBlockingListening(LocIpcRecver& ipcRecver);

    // Create a new LocThread and listen for new messages in it.
    // Calling this function will return immediately and won't block current thread.
    // The listening can be stopped by calling stopNonBlockingListening().
    bool startNonBlockingListening(unique_ptr<LocIpcRecver>& ipcRecver);
    void stopNonBlockingListening();

    // Send out a message.
    // Call this function to send a message in argument data to socket in argument name.
    //
    // Argument name contains the name of the target unix socket. data contains the
    // message to be sent out. Convert your message to a string before calling this function.
    // The function will return true on success, and false on failure.
    static bool send(LocIpcSender& sender, const uint8_t data[],
                     uint32_t length, int32_t msgId = -1);

private:
    static std::string generateThreadName(const std::string& recverName);

private:
    LocThread mThread;
};

/* this is only when client needs to implement Sender / Recver that are not already provided by
   the factor methods prvoided by LocIpc. */

class LocIpcSender {
protected:
    LocIpcSender() = default;
    virtual bool isOperable() const = 0;
    virtual ssize_t send(const uint8_t data[], uint32_t length, int32_t msgId) const = 0;
public:
    virtual ~LocIpcSender() = default;
    inline bool isSendable() const { return isOperable(); }
    inline bool sendData(const uint8_t data[], uint32_t length, int32_t msgId) const {
        return isSendable() && (send(data, length, msgId) > 0);
    }
    virtual unique_ptr<LocIpcRecver> getRecver(const shared_ptr<ILocIpcListener>& listener) {
        return nullptr;
    }
    // Function used to update dest node id and port id for QRTR sender socket
    inline virtual bool updateDestAddr(uint32_t nodeId, uint32_t portId) { return true; }
};

class LocIpcRecver {
    LocIpcSender& mIpcSender;
protected:
    const shared_ptr<ILocIpcListener> mDataCb;
    inline LocIpcRecver(const shared_ptr<ILocIpcListener>& listener, LocIpcSender& sender) :
            mIpcSender(sender), mDataCb(listener) {}
    LocIpcRecver(LocIpcRecver const& recver) = delete;
    LocIpcRecver& operator=(LocIpcRecver const& recver) = delete;
    virtual ssize_t recv() const = 0;
public:
    virtual ~LocIpcRecver() = default;
    inline bool recvData() const { return isRecvable() && (recv() > 0); }
    inline bool isRecvable() const { return mDataCb != nullptr && mIpcSender.isSendable(); }
    virtual void onListenerReady() { if (mDataCb != nullptr) mDataCb->onListenerReady(); }
    inline virtual unique_ptr<LocIpcSender> getLastSender() const {
        return nullptr;
    }
    virtual void abort() const = 0;
    virtual const char* getName() const = 0;
};

class Sock {
    static const char MSG_ABORT[];
    static bool sRandSeeded;
    // in the format of (a little massage needed):
    //   "$MSG_CONCAT_HDR$<unique ID-16bytes>$<msgsize-8bytes>$"
    //   no null ending char needed.
    char LOC_IPC_HEAD[42];
    const uint32_t mMaxTxSize;
    ssize_t sendto(const void *buf, size_t len, int flags, const struct sockaddr *destAddr,
                   socklen_t addrlen) const;
    ssize_t recvfrom(const LocIpcRecver& recver, const shared_ptr<ILocIpcListener>& dataCb,
                     int sid, int flags, struct sockaddr *srcAddr, socklen_t *addrlen) const;
public:
    int mSid;
    //QRTR supports maximum of 16KB packet length over MHI interface.
    //Set it to 15K to ensure that the total packet size
    //(including the header added at the underlying layer) does not exceed 16K
    Sock(int sid, const uint32_t maxTxSize = 15360);
    inline ~Sock() { close(); }
    inline bool isValid() const { return -1 != mSid; }
    ssize_t send(const void *buf, uint32_t len, int flags, const struct sockaddr *destAddr,
                 socklen_t addrlen) const;
    ssize_t recv(const LocIpcRecver& recver, const shared_ptr<ILocIpcListener>& dataCb, int flags,
                 struct sockaddr *srcAddr, socklen_t *addrlen, int sid = -1) const;
    ssize_t sendAbort(int flags, const struct sockaddr *destAddr, socklen_t addrlen);
    inline void close() {
        if (isValid()) {
            ::close(mSid);
            mSid = -1;
        }
    }
};

class SockRecver : public LocIpcRecver {
    shared_ptr<Sock> mSock;
protected:
    inline virtual ssize_t recv() const override {
        return mSock->recv(*this, mDataCb, 0, nullptr, nullptr);
    }
public:
    inline SockRecver(const shared_ptr<ILocIpcListener>& listener,
                  LocIpcSender& sender, shared_ptr<Sock> sock) :
            LocIpcRecver(listener, sender), mSock(sock) {
    }
    inline virtual const char* getName() const override {
        return "SockRecver";
    }
    inline virtual void abort() const override {}
};

}

#endif //__LOC_IPC__
