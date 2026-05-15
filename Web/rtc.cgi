t <html><head><title>SENTINEL-E</title>
t <script>
t function sv(f){if(confirm('¿Guardar nueva configuracion?'))f.submit();}
t </script>
t <style>
t body,table,td,tr,th{background:#050B19!important;color:#E7F1F8!important;}
t body{margin:0;font-family:Verdana;text-align:center;}
t .cnt{width:900px;border:1px solid #153B5A;margin:20px auto;background:#050B19;}
t .bx{background:#081425;border:1px solid #234D70;padding:20px;margin:20px;}
t .hd{color:#12B5F5;font-size:13px;border-bottom:1px solid #16A7D9;padding-bottom:5px;}
t .hd{text-align:left;font-weight:bold;letter-spacing:1px;}
t .tb{width:100%;border-collapse:collapse;margin-top:10px;}
t .tb td{padding:10px 5px;text-align:left;font-size:12px;border-bottom:1px dashed #153B5A;}
t .in{background:#02050A;color:#43D7FF;border:1px solid #16A7D9;padding:6px;width:95%;}
t .in:focus{outline:none;border-color:#FFF;color:#FFF;text-shadow:0 0 5px #FFF;}
t .bt{background:#12B5F5;color:#050B19;border:none;padding:10px 20px;}
t .bt{font-weight:bold;cursor:pointer;letter-spacing:1px;}
t a{color:#43D7FF;text-decoration:none;text-shadow:0 0 5px #43D7FF;font-size:12px;}
t </style></head>
t <body><div class="cnt">
i pg_header.inc
t <div style="padding:15px;">
t <h2 style="color:#12B5F5;text-shadow:0 0 8px #12B5F5;">CONFIGURACION DE SISTEMA</h2>
t <form action="rtc.cgi" method="post">
t <input type="hidden" value="cfg" name="pg">
t <div class="bx">
t <div class="hd">PARAMETROS DE FUNCIONAMIENTO</div>
t <table class="tb">
t <tr><td width=50%>ID Dispositivo (Texto libre)</td>
t <td width=50%><input type="text" name="dev_id" class="in" value="NUCLEO-A"></td></tr>
t <tr><td>Ubicacion / Zona</td>
t <td><input type="text" name="loc" class="in" value="CUADRO_PRINCIPAL"></td></tr>
t </table></div>
t <div class="bx">
t <div class="hd">AJUSTES DE MUESTREO Y ALARMAS</div>
t <table class="tb">
t <tr><td width=50%>Tasa de refresco datos (seg)</td>
t <td width=50%><input type="number" name="ref" class="in" value="5" min="1"></td></tr>
t <tr><td>Umbral Alarma Temp. Max (&deg;C)</td>
t <td><input type="number" name="t_max" class="in" value="45"></td></tr>
t <tr><td>Umbral Alarma Corriente Max (A)</td>
t <td><input type="number" name="i_max" class="in" value="15.0" step="0.1"></td></tr>
t </table></div>
t <div class="bx" style="border-color:#12B5F5;background:#0A1730;">
t <div class="hd" style="color:#FFF;">ADMINISTRACION RFID</div>
t <table class="tb">
t <tr><td width=50%>Nuevo UID Master (Hexadecimal)</td>
t <td width=50%><input type="text" name="rfid_m" class="in" value="A1 B2 C3 D4"></td></tr>
t </table></div>
t <p style="margin:30px 0;">
t <input type="button" class="bt" value="GUARDAR CONFIGURACION" onclick="sv(this.form)">
t &nbsp;&nbsp;
t <input type="reset" value="CANCELAR" style="background:none;color:#43D7FF;border:none;">
t </p></form>
t <div style="border-top:1px solid #153B5A;padding-top:15px;">
t <a href="index.htm">&laquo; VOLVER</a>
t </div></div>
i pg_footer.inc
t </div></body></html>
.