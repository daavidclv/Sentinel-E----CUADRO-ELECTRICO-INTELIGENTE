t <html><head><title>SENTINEL-E</title>
t <style>
t body,table,td,tr,th{background:#050B19!important;color:#E7F1F8!important;}
t body{margin:0;font-family:Verdana;text-align:center;}
t .cnt{width:900px;border:1px solid #153B5A;margin:20px auto;background:#050B19;}
t .bx{background:#081425;border:1px solid #234D70;padding:25px;margin:20px;}
t .lcd{background:#02050A;border:2px solid #16A7D9;padding:20px;margin:15px 0;}
t .in{background:transparent;border:none;border-bottom:1px dashed #43D7FF;}
t .in{color:#43D7FF;font-family:monospace;font-size:24px;width:100%;text-align:center;}
t .in:focus{outline:none;border-bottom:1px solid #FFF;color:#FFF;}
t .bt{background:#12B5F5;color:#050B19;border:none;padding:10px 20px;}
t .bt{font-weight:bold;cursor:pointer;font-size:14px;}
t a{color:#43D7FF;text-decoration:none;text-shadow:0 0 5px #43D7FF;font-size:12px;}
t .ok{color:#00FF00;font-weight:bold;text-shadow:0 0 5px #00FF00;}
t .al{color:#FF3333;font-weight:bold;text-shadow:0 0 5px #FF3333;}
t ul.st{text-align:left;list-style:none;padding:0;margin:0;font-size:14px;line-height:2;}
t ul.st li{border-bottom:1px dashed #153B5A;padding:8px 0;}
t </style></head>
t <body><div class="cnt">
i pg_header.inc
t <div style="padding:15px;">
t <h2 style="color:#12B5F5;text-shadow:0 0 8px #12B5F5;letter-spacing:2px;">ESTADO GLOBAL / LCD</h2>
t <div class="bx" style="text-align:left;">
t <div style="color:#16A7D9;font-size:12px;margin-bottom:10px;font-weight:bold;">DIAGNOSTICO DE RED</div>
t <ul class="st">
t <li>&#9632; NUCLEO-A <span style="float:right;">
c g 1
t </span></li>
t <li>&#9632; NUCLEO-B <span style="float:right;">
c g 2
t </span></li>
t <li>&#9632; Comunicacion bidireccional <span style="float:right;">
c g 3
t </span></li>
t <li>&#9632; Alarmas activas <span style="float:right;">
c g 4
t </span></li>
t </ul>
t </div>
t <div class="bx">
t <p style="font-size:12px;color:#16A7D9;margin-top:0;text-align:left;">CONTROL DISPLAY</p>
t <form action="lcd.cgi" method="post" name="cgi">
t <input type="hidden" value="lcd" name="pg">
t <div class="lcd">
t <div style="font-size:10px;color:#16A7D9;text-align:left;">LINEA 1</div>
c f 1 <input type="text" name="lcd1" class="in" maxlength="20" value="%s">
t <div style="font-size:10px;color:#16A7D9;text-align:left;margin-top:20px;">LINEA 2</div>
c f 2 <input type="text" name="lcd2" class="in" maxlength="20" value="%s">
t </div>
t <p style="margin-top:25px;">
t <input type="submit" class="bt" value="ENVIAR" id="sbm">
t <input type="reset" value="UNDO" style="background:0;color:#43D7FF;border:1px solid #43D7FF;padding:9px;">
t </p></form>
t </div>
t <div style="border-top:1px solid #153B5A;padding-top:15px;">
t <a href="index.htm">&laquo; VOLVER</a>
t </div></div>
i pg_footer.inc
t </div></body></html>
.