t <html><head><title>SENTINEL-E</title>
t <style>
t body,table,td,tr,th{background:#050B19!important;color:#E7F1F8!important;}
t body{margin:0;font-family:Verdana;text-align:center;}
t .cnt{width:900px;border:1px solid #153B5A;margin:20px auto;background:#050B19;}
t .bx{background:#081425;border:1px solid #234D70;padding:25px;margin:20px;}
t ul.st{text-align:left;list-style:none;padding:0;margin:0;line-height:2.5;}
t ul.st li{border-bottom:1px dashed #153B5A;padding:5px 0;font-size:14px;}
t .lbl{color:#16A7D9;font-weight:bold;}
t .val{color:#FFF;font-family:monospace;font-size:16px;float:right;}
t .val{text-shadow:0 0 5px #12B5F5;}
t .ok{color:#00FF00;text-shadow:0 0 5px #00FF00;}
t .wn{color:#FFA500;text-shadow:0 0 5px #FFA500;}
t a{color:#43D7FF;text-decoration:none;text-shadow:0 0 5px #43D7FF;font-size:12px;}
t .bt{background:#12B5F5;color:#050B19;border:none;padding:8px 15px;}
t .bt{font-weight:bold;cursor:pointer;}
t </style></head>
t <body><div class="cnt">
i pg_header.inc
t <div style="padding:15px;">
t <h2 style="color:#12B5F5;text-shadow:0 0 8px #12B5F5;letter-spacing:2px;">ENERGIA NUCLEO-B</h2>
t <div class="bx">
t <div style="color:#16A7D9;font-size:12px;margin-bottom:15px;text-align:left;">
t <b>TELEMETRIA DE POTENCIA</b></div>
t <ul class="st">
t <li><span class="lbl">&#9632; Modo actual</span><span class="val ok">ACTIVO</span></li>
t <li><span class="lbl">&#9632; Consumo instantaneo</span><span class="val">14.2 mA</span></li>
t <li><span class="lbl">&#9632; Tension de pilas</span><span class="val">3.15 V</span></li>
t <li><span class="lbl">&#9632; Tiempo a proximo ciclo</span><span class="val">00:15:30</span></li>
t <li><span class="lbl">&#9632; Nivel de carga estimado</span><span class="val">85%</span></li>
t <li><span class="lbl">&#9632; Temperatura celda</span><span class="val wn">
c t 1
t </span></li>
t </ul>
t </div>
t <div style="margin:20px 0;">
t <input type="button" class="bt" value="REFRESCAR DATOS" onclick="window.location.reload();">
t </div>
t <div style="border-top:1px solid #153B5A;padding-top:15px;">
t <a href="index.htm">&laquo; VOLVER</a>
t </div></div>
i pg_footer.inc
t </div></body></html>
.