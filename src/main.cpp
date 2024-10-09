#include "OV2640.h" 
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>

#include "SimStreamer.h"
#include "OV2640Streamer.h"
#include "CRtspSession.h"

// #define ENABLE_OLED // 如果想使用 OLED，启用此宏
// #define SOFTAP_MODE // 如果要运行自己的软AP，启用此宏,。ap和sta模式不能同时启用，ap是
#define ENABLE_WEBSERVER // 启用 Web 服务器
#define ENABLE_RTSPSERVER // 启用 RTSP 服务器

#ifdef ENABLE_OLED
#include "SSD1306.h"
#define OLED_ADDRESS 0x3c // OLED 地址
#define I2C_SDA 14 // I2C SDA 引脚
#define I2C_SCL 13 // I2C SCL 引脚
SSD1306Wire display(OLED_ADDRESS, I2C_SDA, I2C_SCL, GEOMETRY_128_32);
bool hasDisplay; // 在运行时探测设备
#endif

OV2640 cam; // 创建摄像头对象

#ifdef ENABLE_WEBSERVER
WebServer server(80); // 创建 Web 服务器，端口为 80
#endif

#ifdef ENABLE_RTSPSERVER
WiFiServer rtspServer(8554); // 创建 RTSP 服务器，端口为 8554
#endif

#ifdef SOFTAP_MODE
IPAddress apIP = IPAddress(192, 168, 1, 1); // 软AP 模式下的 IP 地址
#else
#include "wifikeys.h" // 包含 WiFi 密钥文件

#endif

#ifdef ENABLE_WEBSERVER
// 处理 JPG 流媒体
void handle_jpg_stream(void)
{
    WiFiClient client = server.client(); // 获取客户端连接
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    server.sendContent(response); // 发送内容

    while (1)
    {
        cam.run(); // 运行摄像头
        if (!client.connected()) // 检查客户端是否连接
            break;
        response = "--frame\r\n";
        response += "Content-Type: image/jpeg\r\n\r\n";
        server.sendContent(response); // 发送帧内容

        client.write((char *)cam.getfb(), cam.getSize()); // 发送图像数据
        server.sendContent("\r\n");
        if (!client.connected())
            break; // 如果断开连接则退出循环
    }
}

// 处理单个 JPG 请求
void handle_jpg(void)
{
    WiFiClient client = server.client(); // 获取客户端连接

    cam.run(); // 运行摄像头
    if (!client.connected())
    {
        return; // 如果未连接则返回
    }
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-disposition: inline; filename=capture.jpg\r\n"; // 设置响应头
    response += "Content-type: image/jpeg\r\n\r\n";
    server.sendContent(response);
    client.write((char *)cam.getfb(), cam.getSize()); // 发送图像数据
}

// 处理未找到的请求
void handleNotFound()
{
    String message = "Server is running!\n\n";
    message += "URI: ";
    message += server.uri(); // 获取请求的 URI
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST"; // 获取请求方法
    message += "\nArguments: ";
    message += server.args(); // 获取请求参数
    message += "\n";
    server.send(200, "text/plain", message); // 发送响应
}
#endif

// 在 LCD 上显示消息
void lcdMessage(String msg)
{
  #ifdef ENABLE_OLED
    if(hasDisplay) {
        display.clear(); // 清空显示
        display.drawString(128 / 2, 32 / 2, msg); // 绘制消息
        display.display(); // 更新显示
    }
  #endif
}

void setup()
{
  #ifdef ENABLE_OLED
    hasDisplay = display.init(); // 初始化 OLED
    if(hasDisplay) {
        display.flipScreenVertically(); // 反转屏幕
        display.setFont(ArialMT_Plain_16); // 设置字体
        display.setTextAlignment(TEXT_ALIGN_CENTER); // 设置文本对齐方式
    }
  #endif
    lcdMessage("booting"); // 启动消息

    Serial.begin(115200); // 初始化串口
    while (!Serial)
    {
        ;
    }
    cam.init(esp32cam_config); // 初始化摄像头

    IPAddress ip;

#ifdef SOFTAP_MODE
    const char *hostname = "devcam"; // 软AP 名称
    // WiFi.hostname(hostname); // FIXME - 找到未定义的原因
    lcdMessage("starting softAP"); // 启动软AP 消息
    WiFi.mode(WIFI_AP); // 设置为 AP 模式
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0)); // 配置软AP
    bool result = WiFi.softAP(hostname, "12345678", 1, 0); // 启动软AP
    if (!result)
    {
        Serial.println("AP Config failed."); // 配置失败
        return;
    }
    else
    {
        Serial.println("AP Config Success."); // 配置成功
        Serial.print("AP MAC: ");
        Serial.println(WiFi.softAPmacAddress()); // 打印 AP MAC 地址

        ip = WiFi.softAPIP(); // 获取软AP IP 地址
    }
#else
    lcdMessage(String("join ") + ssid); // 连接 WiFi 消息
    WiFi.mode(WIFI_STA); // 设置为 STA 模式
    WiFi.begin(ssid, password); // 连接 WiFi
    while (WiFi.status() != WL_CONNECTED) // 等待连接
    {
        delay(500);
        Serial.print(F(".")); // 打印连接进度
    }
    ip = WiFi.localIP(); // 获取本地 IP 地址
    Serial.println(F("WiFi connected")); // WiFi 连接成功
    Serial.println("");
    Serial.println(ip); // 打印本地 IP 地址
#endif

    lcdMessage(ip.toString()); // 显示 IP 地址

#ifdef ENABLE_WEBSERVER
    server.on("/", HTTP_GET, handle_jpg_stream); // 设置根路径处理函数
    server.on("/jpg", HTTP_GET, handle_jpg); // 设置 JPG 路径处理函数
    server.onNotFound(handleNotFound); // 设置未找到处理函数
    server.begin(); // 启动服务器
#endif

#ifdef ENABLE_RTSPSERVER
    rtspServer.begin(); // 启动 RTSP 服务器
#endif
}

CStreamer *streamer; // 流媒体对象
CRtspSession *session; // RTSP 会话
WiFiClient client; // FIXME, 支持多个客户端

void loop()
{
#ifdef ENABLE_WEBSERVER
    server.handleClient(); // 处理客户端请求
#endif

#ifdef ENABLE_RTSPSERVER
    uint32_t msecPerFrame = 100; // 每帧时间
    static uint32_t lastimage = millis(); // 上一帧时间

    // 如果有活动的客户端连接，处理请求直到断开
    // (FIXME - 支持多个客户端)
    if(session) {
        session->handleRequests(0); // 处理请求

        uint32_t now = millis();
        if(now > lastimage + msecPerFrame || now < lastimage) { // 处理时间溢出
            session->broadcastCurrentFrame(now); // 广播当前帧
            lastimage = now;

            // 检查是否超过最大帧率
            now = millis();
            if(now > lastimage + msecPerFrame)
                printf("warning exceeding max frame rate of %d ms\n", now - lastimage);
        }

        if(session->m_stopped) {
            delete session; // 清理会话
            delete streamer; // 清理流媒体对象
            session = NULL; // 设置为空
            streamer = NULL;
        }
    }
    else {
        client = rtspServer.accept(); // 接受客户端连接

        if(client) {
            //streamer = new SimStreamer(&client, true); // 我们的 UDP/TCP 基于 RTP 的流媒体
            streamer = new OV2640Streamer(&client, cam); // 创建流媒体对象

            session = new CRtspSession(&client, streamer); // 创建 RTSP 会话
        }
    }
#endif
}
