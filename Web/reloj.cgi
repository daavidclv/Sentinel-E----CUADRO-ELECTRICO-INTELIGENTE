t <html><head><title>SENTINEL-E | Reloj</title>
t <script src="xml_http.js"></script>
t <script>
t var fU = new periodicObj("rtc.cgx", 1000);
t function startClock() {
t     updateMultiple(fU); 
t     setTimeout(startClock, 1000);
t }
t window.onload = startClock;
t </script>
t <style>
t body{background:#050B19;color:#E7F1F8;font-family:Verdana;margin:0;}
t body{display:flex;justify-content:center;padding-top:20px;}
t .cnt{width:900px;border:1px solid #153B5A;background:#050B19;}
t .cnt{box-shadow:0 0 20px #000;}
t .hdr{background:#0A1730;border-bottom:2px solid #16A7D9;padding:20px;}
t .hdr{display:flex;align-items:center;justify-content:space-between;}
t .htx h1{color:#16A7D9;margin:0;font-size:36px;letter-spacing:2px;}
t .htx p{color:#CBE5F0;margin:5px 0 0 0;font-size:14px;}
t .nav{background:#081425;border:1px solid #234D70;padding:20px;}
t .nav{margin:20px auto;width:92%;text-align:center;}
t .nav a{color:#43D7FF;text-decoration:none;font-weight:bold;}
t .nav a{font-size:16px;padding:0 10px;}
t .nav a:hover{color:#FFF;text-shadow:0 0 8px #43D7FF;}
t .sep{color:#234D70;}
t .crd{background:#0A1730;border:1px solid #16A7D9;padding:30px;}
t .crd{width:300px;display:inline-block;margin:20px;}
t .val{font-size:36px;font-family:monospace;font-weight:bold;}
t .val{color:#FFF;text-shadow:0 0 10px #12B5F5;}
t .lbl{color:#16A7D9;font-size:14px;letter-spacing:2px;margin-bottom:10px;}
t .ftr{background:#08101E;border-top:1px solid #163B59;padding:15px;}
t .ftr{font-size:12px;color:#AFC4D2;text-align:center;}
t </style></head>
t <body><div class="cnt">
t <div class="hdr">
t <div class="htx">
t <h1>SENTINEL-E</h1>
t <p>Reloj del Sistema (NTP / RTC)</p>
t </div>
t <img src="logo.gif" width="80" alt="Sentinel">
t </div>
t <div class="nav">
t <a href="/index.htm">Dashboard</a><span class="sep">|</span>
t <a href="/system.cgi">Control</a><span class="sep">|</span>
t <a href="/leds.cgi">Estado global</a><span class="sep">|</span>
t <a href="/energia.cgi">Energ&iacute;a</a><span class="sep">|</span>
t <a href="/config.cgi">Configuraci&oacute;n</a>
t </div>
t <div style="padding:30px;text-align:center;">
t   <div class="crd">
t     <div class="lbl">FECHA ACTUAL</div>
t     <input type="text" id="rtc_date" class="val" value="--/--/----" 
t            readonly style="background:transparent; border:none; 
t            text-align:center; width:100%;">
t   </div>
t   <div class="crd">
t     <div class="lbl">HORA LOCAL</div>
t     <input type="text" id="rtc_time" class="val" value="--:--:--" 
t            readonly style="background:transparent; border:none; 
t            text-align:center; width:100%;">
t   </div>
t </div>
t <p style="font-size:12px; color:#43D7FF; text-align:center; 
t           padding-bottom:20px;">
t   * La hora se sincroniza mediante SNTP y se mantiene con el RTC.
t </p>
t <div class="ftr">
t   SENTINEL-E &copy; 2026 - Interfaz de supervisi&oacute;n
t </div>
t </div></body></html>
.