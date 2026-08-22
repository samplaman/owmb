#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_core/juce_core.h>
 #include <juce_events/juce_events.h>
#endif

#include <vector>
#include <functional>
#include <atomic>
#include <memory>

namespace openwav
{

struct MobileRecordingItem
{
    juce::String name;
    juce::int64 sizeBytes { 0 };
    juce::String sizeFormatted;
    juce::String date;
    juce::String downloadUrl;
    double durationSeconds { 0.0 };
    bool isAlreadyDownloaded { false };
};

struct DiscoveredMobileDevice
{
    juce::String ipAddress;
    int port { 7777 };
    juce::String deviceName;
    juce::Time lastSeen;
};

class MobileSyncListener
{
public:
    virtual ~MobileSyncListener() = default;
    virtual void discoveredDevicesChanged() {}
    virtual void fileReceivedFromPhone(const juce::File& file) {}
};

class MobileSyncManager : public juce::Thread,
                          public juce::Timer
{
public:
    MobileSyncManager(int defaultServicePort = 7777, int discoveryPort = 7778);
    ~MobileSyncManager() override;

    void startServices();
    void stopServices();

    // Device Discovery
    std::vector<DiscoveredMobileDevice> getDiscoveredDevices() const;
    void broadcastDesktopBeacon();
    void scanLocalSubnet(std::function<void(bool foundAny)> onFinished = nullptr);
    bool probeDevice(const juce::String& host, int port);
    juce::String getLocalIpAddress() const;
    static bool isPrivateLanIp(const juce::String& ip);

    // REST Client Operations
    void fetchRecordings(const juce::String& host, int port,
                         std::function<void(bool success, const std::vector<MobileRecordingItem>& items, const juce::String& error)> callback);

    void downloadRecording(const juce::String& host, int port,
                           const juce::String& remoteFileName,
                           const juce::File& destinationFile,
                           std::function<void(float progress)> progressCallback,
                           std::function<void(bool success, const juce::File& downloadedFile, const juce::String& error)> completionCallback);

    void streamPreview(const juce::String& host, int port,
                       const juce::String& remoteFileName,
                       std::function<void(bool success, const juce::File& tempFile, const juce::String& error)> completionCallback);

    // Push Receiver Server (Allows Phone to push directly to Desktop)
    bool isReceiverRunning() const noexcept { return receiverRunning.load(); }
    int getReceiverPort() const noexcept { return activeReceiverPort; }
    static juce::File getRecordingsDirectory();

    // Listeners
    void addListener(MobileSyncListener* listener);
    void removeListener(MobileSyncListener* listener);

    void run() override;
    void timerCallback() override;

private:
    int localDiscoveryPort { 7778 };
    int targetServicePort { 7777 };
    int activeReceiverPort { 7777 };

    std::atomic<bool> shouldStop { false };
    std::atomic<bool> receiverRunning { false };

    // Sockets
    std::unique_ptr<juce::DatagramSocket> udpDiscoverySocket;
    std::unique_ptr<juce::StreamingSocket> tcpReceiverSocket;

    // Background push receiver thread
    class PushReceiverThread : public juce::Thread
    {
    public:
        PushReceiverThread(MobileSyncManager& owner)
            : juce::Thread("OWMB Mobile Push Receiver"), owner(owner) {}
        void run() override { owner.runReceiverLoop(); }
    private:
        MobileSyncManager& owner;
    };

    std::unique_ptr<PushReceiverThread> receiverThread;

    void runReceiverLoop();
    void handleIncomingPushConnection(std::unique_ptr<juce::StreamingSocket> client);

    mutable juce::CriticalSection devicesLock;
    std::vector<DiscoveredMobileDevice> discoveredDevices;

    juce::ListenerList<MobileSyncListener> listeners;

    static std::vector<MobileRecordingItem> parseRecordingsJson(const juce::String& json);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MobileSyncManager)
};

} // namespace openwav
