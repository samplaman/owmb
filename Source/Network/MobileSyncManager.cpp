#include "MobileSyncManager.h"
#include <algorithm>
#include <cstring>

namespace openwav
{

MobileSyncManager::MobileSyncManager(int defaultServicePort, int discoveryPort)
    : juce::Thread("OWMB Mobile Discovery Listener"),
      localDiscoveryPort(discoveryPort),
      targetServicePort(defaultServicePort),
      activeReceiverPort(defaultServicePort)
{
}

MobileSyncManager::~MobileSyncManager()
{
    stopServices();
}

void MobileSyncManager::startServices()
{
    stopServices();

    shouldStop.store(false);

    // 1. Start UDP Discovery Listener & Broadcast
    udpDiscoverySocket.reset(new juce::DatagramSocket(true));
    udpDiscoverySocket->bindToPort(localDiscoveryPort);
    startThread(juce::Thread::Priority::normal);

    // 2. Start Embedded TCP Push Receiver (Listening for direct uploads from Phone)
    tcpReceiverSocket.reset(new juce::StreamingSocket());
    bool bound = tcpReceiverSocket->createListener(targetServicePort, "");
    if (!bound)
    {
        // If 7777 is occupied, fallback to 7779
        activeReceiverPort = targetServicePort + 2;
        bound = tcpReceiverSocket->createListener(activeReceiverPort, "");
    }
    else
    {
        activeReceiverPort = targetServicePort;
    }

    if (bound)
    {
        receiverRunning.store(true);
        receiverThread.reset(new PushReceiverThread(*this));
        receiverThread->startThread(juce::Thread::Priority::normal);
    }

    // 3. Start Periodic Beacon Broadcast (every 2 seconds)
    startTimer(2000);
    broadcastDesktopBeacon();
}

void MobileSyncManager::stopServices()
{
    stopTimer();
    shouldStop.store(true);

    if (udpDiscoverySocket != nullptr)
    {
        udpDiscoverySocket->shutdown();
    }
    stopThread(1000);
    udpDiscoverySocket.reset();

    receiverRunning.store(false);
    if (tcpReceiverSocket != nullptr)
    {
        tcpReceiverSocket->close();
    }

    if (receiverThread != nullptr)
    {
        receiverThread->stopThread(1000);
        receiverThread.reset();
    }
    tcpReceiverSocket.reset();
}

juce::File MobileSyncManager::getRecordingsDirectory()
{
    auto userDocs = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto recDir = userDocs.getChildFile("OWMB_Recordings");
    if (!recDir.exists())
        recDir.createDirectory();
    return recDir;
}

juce::String MobileSyncManager::getLocalIpAddress() const
{
    auto allAddresses = juce::IPAddress::getAllAddresses();

    for (const auto& addr : allAddresses)
    {
        auto ipStr = addr.toString();
        if (ipStr.startsWith("192.168."))
            return ipStr;
    }

    for (const auto& addr : allAddresses)
    {
        auto ipStr = addr.toString();
        if (ipStr.startsWith("10."))
            return ipStr;
    }

    for (const auto& addr : allAddresses)
    {
        auto ipStr = addr.toString();
        if (isPrivateLanIp(ipStr) && ipStr != "127.0.0.1")
            return ipStr;
    }

    return "127.0.0.1";
}

bool MobileSyncManager::isPrivateLanIp(const juce::String& ip)
{
    if (ip.isEmpty() || ip == "127.0.0.1" || ip == "localhost" || ip.startsWith("::1") || ip.startsWith("fe80:"))
        return true;

    if (ip.endsWithIgnoreCase(".local") || ip.endsWithIgnoreCase(".lan") || ip.endsWithIgnoreCase(".home") || !ip.contains("."))
        return true;

    if (ip.startsWith("192.168."))
        return true;

    if (ip.startsWith("10."))
        return true;

    if (ip.startsWith("172."))
    {
        auto parts = juce::StringArray::fromTokens(ip, ".", "");
        if (parts.size() >= 2)
        {
            int second = parts[1].getIntValue();
            if (second >= 16 && second <= 31)
                return true;
        }
    }

    return false;
}

std::vector<DiscoveredMobileDevice> MobileSyncManager::getDiscoveredDevices() const
{
    const juce::ScopedLock sl(devicesLock);
    return discoveredDevices;
}

void MobileSyncManager::broadcastDesktopBeacon()
{
    if (udpDiscoverySocket == nullptr) return;

    auto localHost = juce::SystemStats::getComputerName();
    if (localHost.isEmpty()) localHost = "Desktop OWMB";

    juce::String msg = "OWMB_BEACON:DESKTOP:" + juce::String(activeReceiverPort) + ":" + localHost;

    // Send to global broadcast address
    udpDiscoverySocket->write("255.255.255.255", localDiscoveryPort, msg.toRawUTF8(), static_cast<int>(msg.getNumBytesAsUTF8()));

    // Also send to all local subnet broadcast addresses
    for (const auto& addr : juce::IPAddress::getAllAddresses())
    {
        auto ip = addr.toString();
        if (isPrivateLanIp(ip) && ip != "127.0.0.1")
        {
            auto subnet = ip.upToLastOccurrenceOf(".", false, false) + ".255";
            udpDiscoverySocket->write(subnet, localDiscoveryPort, msg.toRawUTF8(), static_cast<int>(msg.getNumBytesAsUTF8()));
        }
    }
}

bool MobileSyncManager::probeDevice(const juce::String& host, int port)
{
    juce::StreamingSocket socket;
    if (socket.connect(host, port, 300)) // 300ms connect timeout
    {
        juce::String req = "GET /api/status HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
        if (socket.write(req.toRawUTF8(), static_cast<int>(req.getNumBytesAsUTF8())) > 0)
        {
            if (socket.waitUntilReady(true, 800) > 0)
            {
                char buf[512];
                int r = socket.read(buf, sizeof(buf) - 1, true);
                if (r > 0)
                {
                    buf[r] = '\0';
                    juce::String resp(buf);
                    if (resp.contains("OWMB_LAN_SYNC") || resp.contains("200 OK") || resp.contains("200"))
                    {
                        DiscoveredMobileDevice dev;
                        dev.ipAddress = host;
                        dev.port = port;
                        dev.deviceName = "OWMB Field Recorder";
                        dev.lastSeen = juce::Time::getCurrentTime();

                        bool isNew = false;
                        {
                            const juce::ScopedLock sl(devicesLock);
                            bool found = false;
                            for (auto& existing : discoveredDevices)
                            {
                                if (existing.ipAddress == host)
                                {
                                    existing.lastSeen = juce::Time::getCurrentTime();
                                    found = true;
                                    break;
                                }
                            }
                            if (!found)
                            {
                                discoveredDevices.push_back(dev);
                                isNew = true;
                            }
                        }

                        if (isNew)
                        {
                            juce::MessageManager::callAsync([this]() {
                                listeners.call(&MobileSyncListener::discoveredDevicesChanged);
                            });
                        }
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void MobileSyncManager::scanLocalSubnet(std::function<void(bool foundAny)> onFinished)
{
    juce::Thread::launch([this, onFinished]() {
        // First probe localhost in case testing on same machine
        probeDevice("127.0.0.1", targetServicePort);

        auto localIp = getLocalIpAddress();
        if (localIp.isEmpty() || localIp == "127.0.0.1")
        {
            if (onFinished != nullptr)
            {
                juce::MessageManager::callAsync([this, onFinished]() {
                    onFinished(!getDiscoveredDevices().empty());
                });
            }
            return;
        }

        auto prefix = localIp.upToLastOccurrenceOf(".", false, false) + ".";
        int myLastOctet = localIp.fromLastOccurrenceOf(".", false, false).getIntValue();

        auto completedCount = std::make_shared<std::atomic<int>>(0);
        auto foundAny = std::make_shared<std::atomic<bool>>(!getDiscoveredDevices().empty());

        const int numProbes = 254;
        const int numThreads = 16;
        int perThread = (numProbes + numThreads - 1) / numThreads;

        for (int t = 0; t < numThreads; ++t)
        {
            int startIdx = t * perThread + 1;
            int endIdx = std::min(254, (t + 1) * perThread);

            juce::Thread::launch([this, prefix, startIdx, endIdx, myLastOctet, completedCount, foundAny, numThreads, onFinished]() {
                for (int i = startIdx; i <= endIdx && !shouldStop.load(); ++i)
                {
                    if (i == myLastOctet) continue;
                    juce::String targetIp = prefix + juce::String(i);
                    if (probeDevice(targetIp, targetServicePort))
                    {
                        foundAny->store(true);
                    }
                }

                if (++(*completedCount) == numThreads)
                {
                    if (onFinished != nullptr)
                    {
                        bool found = foundAny->load();
                        juce::MessageManager::callAsync([onFinished, found]() {
                            onFinished(found);
                        });
                    }
                }
            });
        }
    });
}

void MobileSyncManager::timerCallback()
{
    // 1. Broadcast beacon
    broadcastDesktopBeacon();

    // 2. Prune devices not heard from in 15 seconds
    bool changed = false;
    {
        const juce::ScopedLock sl(devicesLock);
        auto now = juce::Time::getCurrentTime();
        auto it = discoveredDevices.begin();
        while (it != discoveredDevices.end())
        {
            if ((now - it->lastSeen).inSeconds() > 15)
            {
                it = discoveredDevices.erase(it);
                changed = true;
            }
            else
            {
                ++it;
            }
        }
    }

    if (changed)
    {
        listeners.call(&MobileSyncListener::discoveredDevicesChanged);
    }
}

void MobileSyncManager::run()
{
    char buffer[1024];

    while (!threadShouldExit() && !shouldStop.load())
    {
        if (udpDiscoverySocket == nullptr)
            break;

        juce::String senderIp;
        int senderPort = 0;

        int bytesRead = udpDiscoverySocket->read(buffer, sizeof(buffer) - 1, false, senderIp, senderPort);

        if (bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            juce::String packet(buffer);

            if (packet.startsWith("OWMB_BEACON:RECORDER:"))
            {
                auto tokens = juce::StringArray::fromTokens(packet, ":", "");
                if (tokens.size() >= 3)
                {
                    int mobileServicePort = tokens[2].getIntValue();
                    if (mobileServicePort <= 0) mobileServicePort = 7777;

                    juce::String deviceName = tokens.size() >= 4 ? tokens[3] : "OWMB Field Recorder";

                    bool isNewOrUpdated = false;
                    {
                        const juce::ScopedLock sl(devicesLock);
                        bool found = false;
                        for (auto& dev : discoveredDevices)
                        {
                            if (dev.ipAddress == senderIp)
                            {
                                dev.port = mobileServicePort;
                                dev.deviceName = deviceName;
                                dev.lastSeen = juce::Time::getCurrentTime();
                                found = true;
                                break;
                            }
                        }

                        if (!found)
                        {
                            DiscoveredMobileDevice newDev;
                            newDev.ipAddress = senderIp;
                            newDev.port = mobileServicePort;
                            newDev.deviceName = deviceName;
                            newDev.lastSeen = juce::Time::getCurrentTime();
                            discoveredDevices.push_back(newDev);
                            isNewOrUpdated = true;
                        }
                    }

                    if (isNewOrUpdated)
                    {
                        juce::MessageManager::callAsync([this]() {
                            listeners.call(&MobileSyncListener::discoveredDevicesChanged);
                        });
                    }
                }
            }
        }
        else
        {
            juce::Thread::sleep(50);
        }
    }
}

void MobileSyncManager::runReceiverLoop()
{
    while (!shouldStop.load() && receiverRunning.load())
    {
        if (tcpReceiverSocket == nullptr)
            break;

        auto* client = tcpReceiverSocket->waitForNextConnection();
        if (client != nullptr)
        {
            std::unique_ptr<juce::StreamingSocket> clientSock(client);
            handleIncomingPushConnection(std::move(clientSock));
        }
        else
        {
            juce::Thread::sleep(30);
        }
    }
}

void MobileSyncManager::handleIncomingPushConnection(std::unique_ptr<juce::StreamingSocket> client)
{
    if (client == nullptr) return;

    auto clientHost = client->getHostName().trim();
    if (!clientHost.isEmpty() && !isPrivateLanIp(clientHost))
    {
        juce::String resp = "HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n";
        client->write(resp.toRawUTF8(), static_cast<int>(resp.getNumBytesAsUTF8()));
        return;
    }

    // Read HTTP Header
    juce::MemoryBlock headerBlock;
    char buf[1024];
    int headerEndPos = -1;

    while (headerEndPos == -1)
    {
        int bytesRead = client->read(buf, sizeof(buf), false);
        if (bytesRead <= 0) break;

        headerBlock.append(buf, static_cast<size_t>(bytesRead));

        const char* data = static_cast<const char*>(headerBlock.getData());
        int size = static_cast<int>(headerBlock.getSize());

        for (int i = 0; i < size - 3; ++i)
        {
            if (data[i] == '\r' && data[i+1] == '\n' && data[i+2] == '\r' && data[i+3] == '\n')
            {
                headerEndPos = i + 4;
                break;
            }
        }

        if (headerBlock.getSize() > 16384) break; // Limit header size
    }

    if (headerEndPos == -1) return;

    juce::String headerStr(static_cast<const char*>(headerBlock.getData()), static_cast<size_t>(headerEndPos));
    auto firstLine = headerStr.upToFirstOccurrenceOf("\r\n", false, false);
    auto tokens = juce::StringArray::fromTokens(firstLine, " ", "");

    if (tokens.size() < 2 || !tokens[0].equalsIgnoreCase("POST"))
    {
        juce::String resp = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n";
        client->write(resp.toRawUTF8(), static_cast<int>(resp.getNumBytesAsUTF8()));
        return;
    }

    auto pathAndQuery = tokens[1];
    auto path = pathAndQuery.upToFirstOccurrenceOf("?", false, false);
    auto query = pathAndQuery.fromFirstOccurrenceOf("?", false, false);

    if (path != "/api/upload")
    {
        juce::String resp = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
        client->write(resp.toRawUTF8(), static_cast<int>(resp.getNumBytesAsUTF8()));
        return;
    }

    auto filename = query.fromFirstOccurrenceOf("file=", false, false).upToFirstOccurrenceOf("&", false, false);
    filename = juce::URL::removeEscapeChars(filename);
    filename = juce::File::createLegalFileName(filename);
    if (filename.isEmpty())
        filename = "Phone_Rec_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S") + ".wav";
    if (!filename.endsWithIgnoreCase(".wav"))
        filename += ".wav";

    // Extract Content-Length
    juce::int64 contentLength = -1;
    auto lines = juce::StringArray::fromLines(headerStr);
    for (const auto& line : lines)
    {
        if (line.startsWithIgnoreCase("Content-Length:"))
        {
            contentLength = line.fromFirstOccurrenceOf(":", false, false).trim().getLargeIntValue();
            break;
        }
    }

    auto destDir = getRecordingsDirectory();
    auto destFile = destDir.getChildFile(filename);
    if (destFile.existsAsFile())
        destFile.deleteFile();

    std::unique_ptr<juce::FileOutputStream> fos(destFile.createOutputStream());
    if (fos == nullptr || !fos->openedOk())
    {
        juce::String resp = "HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\n\r\n";
        client->write(resp.toRawUTF8(), static_cast<int>(resp.getNumBytesAsUTF8()));
        return;
    }

    // Write remainder of headerBlock after headerEndPos to file
    int extraBytes = static_cast<int>(headerBlock.getSize()) - headerEndPos;
    if (extraBytes > 0)
    {
        fos->write(static_cast<const char*>(headerBlock.getData()) + headerEndPos, static_cast<size_t>(extraBytes));
    }

    juce::int64 totalReceived = extraBytes;
    char fileBuf[32768];

    while (contentLength < 0 || totalReceived < contentLength)
    {
        int toRead = sizeof(fileBuf);
        if (contentLength > 0 && (contentLength - totalReceived) < static_cast<juce::int64>(toRead))
            toRead = static_cast<int>(contentLength - totalReceived);

        int bytesRead = client->read(fileBuf, toRead, false);
        if (bytesRead <= 0) break;

        fos->write(fileBuf, static_cast<size_t>(bytesRead));
        totalReceived += bytesRead;
    }

    fos->flush();
    fos.reset();

    // Respond OK
    juce::String okResp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"status\":\"ok\",\"savedFile\":\"" + destFile.getFileName() + "\"}";
    client->write(okResp.toRawUTF8(), static_cast<int>(okResp.getNumBytesAsUTF8()));

    // Notify Listeners on MessageManager thread
    juce::MessageManager::callAsync([this, destFile]() {
        listeners.call(&MobileSyncListener::fileReceivedFromPhone, destFile);
    });
}

void MobileSyncManager::fetchRecordings(const juce::String& host, int port,
                                       std::function<void(bool success, const std::vector<MobileRecordingItem>& items, const juce::String& error)> callback)
{
    juce::Thread::launch([host, port, callback]() {
        juce::StreamingSocket socket;
        if (!socket.connect(host, port, 3000))
        {
            juce::MessageManager::callAsync([callback, host, port]() {
                callback(false, {}, "Could not connect to Field Recorder at " + host + ":" + juce::String(port));
            });
            return;
        }

        juce::String request = "GET /api/list HTTP/1.1\r\n"
                               "Host: " + host + "\r\n"
                               "User-Agent: OWMB-Desktop/1.0\r\n"
                               "Accept: application/json\r\n"
                               "Connection: close\r\n\r\n";

        if (socket.write(request.toRawUTF8(), static_cast<int>(request.getNumBytesAsUTF8())) < 0)
        {
            juce::MessageManager::callAsync([callback]() {
                callback(false, {}, "Failed to send request to mobile device.");
            });
            return;
        }

        juce::MemoryOutputStream responseStream;
        char buf[4096];

        if (socket.waitUntilReady(true, 5000) <= 0)
        {
            juce::MessageManager::callAsync([callback]() {
                callback(false, {}, "Timed out waiting for response from phone.");
            });
            return;
        }

        while (true)
        {
            int bytesRead = socket.read(buf, sizeof(buf), true);
            if (bytesRead <= 0) break;
            responseStream.write(buf, static_cast<size_t>(bytesRead));

            auto currentStr = responseStream.toString();
            auto headerEnd = currentStr.indexOf("\r\n\r\n");
            if (headerEnd < 0)
                headerEnd = currentStr.indexOf("\n\n");

            if (headerEnd >= 0)
            {
                auto headers = currentStr.substring(0, headerEnd);
                auto lines = juce::StringArray::fromLines(headers);
                juce::int64 contentLength = -1;
                for (const auto& line : lines)
                {
                    if (line.startsWithIgnoreCase("Content-Length:"))
                    {
                        contentLength = line.fromFirstOccurrenceOf(":", false, false).trim().getLargeIntValue();
                        break;
                    }
                }

                if (contentLength >= 0)
                {
                    int bodyStart = headerEnd + (currentStr.contains("\r\n\r\n") ? 4 : 2);
                    int bodyReceived = static_cast<int>(responseStream.getDataSize()) - bodyStart;
                    if (bodyReceived >= contentLength)
                        break;
                }
            }

            if (socket.waitUntilReady(true, 250) <= 0)
                break;
        }

        juce::String responseText = responseStream.toString();
        auto header = responseText.upToFirstOccurrenceOf("\r\n\r\n", false, false);
        if (header.isEmpty())
            header = responseText.upToFirstOccurrenceOf("\n\n", false, false);

        auto firstLine = header.upToFirstOccurrenceOf("\r\n", false, false).trim();
        if (firstLine.isEmpty())
            firstLine = header.upToFirstOccurrenceOf("\n", false, false).trim();

        if (!firstLine.contains("200"))
        {
            auto errorBody = responseText.fromFirstOccurrenceOf("\r\n\r\n", false, false).trim();
            if (errorBody.isEmpty())
                errorBody = responseText.fromFirstOccurrenceOf("\n\n", false, false).trim();
            if (errorBody.isEmpty())
                errorBody = firstLine.isNotEmpty() ? firstLine : "Unknown HTTP error (Empty response)";

            juce::MessageManager::callAsync([callback, errorBody]() {
                callback(false, {}, "Device error: " + errorBody);
            });
            return;
        }

        auto body = responseText.fromFirstOccurrenceOf("\r\n\r\n", false, false);
        if (body.isEmpty())
            body = responseText.fromFirstOccurrenceOf("\n\n", false, false);

        auto items = parseRecordingsJson(body);

        // Check if files already exist locally in OWMB_Recordings
        auto recDir = getRecordingsDirectory();
        for (auto& item : items)
        {
            if (recDir.getChildFile(item.name).existsAsFile())
            {
                item.isAlreadyDownloaded = true;
            }
        }

        juce::MessageManager::callAsync([callback, items]() {
            callback(true, items, {});
        });
    });
}

void MobileSyncManager::downloadRecording(const juce::String& host, int port,
                                         const juce::String& remoteFileName,
                                         const juce::File& destinationFile,
                                         std::function<void(float progress)> progressCallback,
                                         std::function<void(bool success, const juce::File& downloadedFile, const juce::String& error)> completionCallback)
{
    juce::Thread::launch([host, port, remoteFileName, destinationFile, progressCallback, completionCallback]() {
        juce::StreamingSocket socket;
        if (!socket.connect(host, port, 4000))
        {
            juce::MessageManager::callAsync([completionCallback, host, port]() {
                completionCallback(false, {}, "Failed to connect to " + host + ":" + juce::String(port));
            });
            return;
        }

        auto escapedName = juce::URL::addEscapeChars(remoteFileName, true);
        juce::String request = "GET /api/download?file=" + escapedName + " HTTP/1.1\r\n"
                               "Host: " + host + "\r\n"
                               "User-Agent: OWMB-Desktop/1.0\r\n"
                               "Connection: close\r\n\r\n";

        if (socket.write(request.toRawUTF8(), static_cast<int>(request.getNumBytesAsUTF8())) < 0)
        {
            juce::MessageManager::callAsync([completionCallback]() {
                completionCallback(false, {}, "Socket write error during download.");
            });
            return;
        }

        if (socket.waitUntilReady(true, 5000) <= 0)
        {
            juce::MessageManager::callAsync([completionCallback]() {
                completionCallback(false, {}, "Device timed out waiting for audio data.");
            });
            return;
        }

        // Read HTTP headers
        juce::MemoryBlock headerBlock;
        char buf[4096];
        int headerEndPos = -1;

        while (headerEndPos == -1)
        {
            int bytesRead = socket.read(buf, sizeof(buf), true);
            if (bytesRead <= 0) break;

            headerBlock.append(buf, static_cast<size_t>(bytesRead));

            const char* data = static_cast<const char*>(headerBlock.getData());
            int size = static_cast<int>(headerBlock.getSize());

            for (int i = 0; i < size - 3; ++i)
            {
                if (data[i] == '\r' && data[i+1] == '\n' && data[i+2] == '\r' && data[i+3] == '\n')
                {
                    headerEndPos = i + 4;
                    break;
                }
            }

            if (headerBlock.getSize() > 16384) break;
        }

        if (headerEndPos == -1)
        {
            juce::MessageManager::callAsync([completionCallback]() {
                completionCallback(false, {}, "Invalid HTTP response from mobile device.");
            });
            return;
        }

        juce::String headerStr(static_cast<const char*>(headerBlock.getData()), static_cast<size_t>(headerEndPos));
        if (!headerStr.contains("200 OK") && !headerStr.contains("HTTP/1.1 200") && !headerStr.contains("HTTP/1.0 200"))
        {
            juce::MessageManager::callAsync([completionCallback, headerStr]() {
                completionCallback(false, {}, "File not found or error on device: " + headerStr.upToFirstOccurrenceOf("\r\n", false, false));
            });
            return;
        }

        // Parse Content-Length
        juce::int64 contentLength = -1;
        auto lines = juce::StringArray::fromLines(headerStr);
        for (const auto& line : lines)
        {
            if (line.startsWithIgnoreCase("Content-Length:"))
            {
                contentLength = line.fromFirstOccurrenceOf(":", false, false).trim().getLargeIntValue();
                break;
            }
        }

        if (destinationFile.existsAsFile())
            destinationFile.deleteFile();

        std::unique_ptr<juce::FileOutputStream> fos(destinationFile.createOutputStream());
        if (fos == nullptr || !fos->openedOk())
        {
            juce::MessageManager::callAsync([completionCallback]() {
                completionCallback(false, {}, "Could not create destination file on disk.");
            });
            return;
        }

        // Write leftover bytes from header block
        int extraBytes = static_cast<int>(headerBlock.getSize()) - headerEndPos;
        if (extraBytes > 0)
        {
            fos->write(static_cast<const char*>(headerBlock.getData()) + headerEndPos, static_cast<size_t>(extraBytes));
        }

        juce::int64 totalDownloaded = extraBytes;
        char fileChunk[32768];

        while (contentLength < 0 || totalDownloaded < contentLength)
        {
            int toRead = sizeof(fileChunk);
            if (contentLength > 0 && (contentLength - totalDownloaded) < static_cast<juce::int64>(toRead))
                toRead = static_cast<int>(contentLength - totalDownloaded);

            int bytesRead = socket.read(fileChunk, toRead, true);
            if (bytesRead <= 0) break;

            fos->write(fileChunk, static_cast<size_t>(bytesRead));
            totalDownloaded += bytesRead;

            if (contentLength > 0 && progressCallback != nullptr)
            {
                float prog = juce::jlimit(0.0f, 1.0f, static_cast<float>(totalDownloaded) / static_cast<float>(contentLength));
                juce::MessageManager::callAsync([progressCallback, prog]() {
                    progressCallback(prog);
                });
            }
        }

        fos->flush();
        fos.reset();

        juce::MessageManager::callAsync([completionCallback, destinationFile]() {
            completionCallback(true, destinationFile, {});
        });
    });
}

void MobileSyncManager::streamPreview(const juce::String& host, int port,
                                     const juce::String& remoteFileName,
                                     std::function<void(bool success, const juce::File& tempFile, const juce::String& error)> completionCallback)
{
    auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("owmb_preview_" + remoteFileName);
    downloadRecording(host, port, remoteFileName, tempFile, nullptr, completionCallback);
}

void MobileSyncManager::addListener(MobileSyncListener* listener)
{
    listeners.add(listener);
}

void MobileSyncManager::removeListener(MobileSyncListener* listener)
{
    listeners.remove(listener);
}

std::vector<MobileRecordingItem> MobileSyncManager::parseRecordingsJson(const juce::String& json)
{
    std::vector<MobileRecordingItem> list;
    auto varResult = juce::JSON::parse(json);

    juce::Array<juce::var>* arrayPtr = nullptr;

    if (varResult.isArray())
    {
        arrayPtr = varResult.getArray();
    }
    else if (varResult.isObject())
    {
        if (auto* obj = varResult.getDynamicObject())
        {
            if (obj->hasProperty("files") && obj->getProperty("files").isArray())
            {
                arrayPtr = obj->getProperty("files").getArray();
            }
        }
    }

    if (arrayPtr != nullptr)
    {
        for (const auto& itemVar : *arrayPtr)
        {
            if (auto* itemObj = itemVar.getDynamicObject())
            {
                MobileRecordingItem rec;
                rec.name = itemObj->getProperty("name").toString();
                rec.sizeBytes = itemObj->getProperty("sizeBytes").toString().getLargeIntValue();
                if (rec.sizeBytes <= 0)
                    rec.sizeBytes = itemObj->getProperty("size").toString().getLargeIntValue();

                rec.sizeFormatted = itemObj->getProperty("sizeFormatted").toString();
                if (rec.sizeFormatted.isEmpty() && rec.sizeBytes > 0)
                {
                    double mb = static_cast<double>(rec.sizeBytes) / (1024.0 * 1024.0);
                    rec.sizeFormatted = juce::String(mb, 2) + " MB";
                }

                rec.date = itemObj->getProperty("date").toString();
                rec.downloadUrl = itemObj->getProperty("downloadUrl").toString();
                if (rec.downloadUrl.isEmpty())
                    rec.downloadUrl = itemObj->getProperty("url").toString();

                rec.durationSeconds = itemObj->getProperty("durationSeconds");

                if (rec.name.isNotEmpty())
                    list.push_back(rec);
            }
        }
    }

    return list;
}

} // namespace openwav
