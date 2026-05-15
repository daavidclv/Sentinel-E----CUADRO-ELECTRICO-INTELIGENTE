t <html><head><title>SENTINEL-E | Logs</title>
t <script>
t function bd(){
t   var msg = '\xbfBORRAR TODO el historial?\n';
t   msg += 'Esta accion NO se puede deshacer.';
t   if(confirm(msg)){
t     document.getElementById('fb').submit();
t   }
t }
t </script>
t <style>
t body,table,td,tr,th{background:#050B19!important;color:#E7F1F8!important;}
t body{margin:0;font-family:Verdana;text-align:center;}
t .cnt{width:900px;border:1px solid #153B5A;margin:20px auto;}
t .term{background:#02050A!important;border:1px solid #16A7D9;}
t .term{height:300px;overflow-y:auto;margin:20px;padding:10px;}
t .tb{width:100%;border-collapse:collapse;font-family:monospace;}
t .tb th{background:#0A1730!important;color:#43D7FF!important;padding:10px;}
t .tb th{border-bottom:2px solid #16A7D9;text-align:left;}
t .tb td{border-bottom:1px dashed #153B5A;padding:8px;text-align:left;}
t .bt{background:#12B5F5;color:#050B19;border:none;padding:8px 15px;}
t .bt{font-weight:bold;cursor:pointer;}
t .btr{background:#FF3333;color:#FFF;border:none;padding:8px 15px;}
t .btr{font-weight:bold;cursor:pointer;margin-left:10px;}
t .info{color:#43D7FF;font-size:13px;margin:10px 0;}
t a{color:#43D7FF;text-decoration:none;text-shadow:0 0 5px #43D7FF;}
t </style></head>
t <body><div class="cnt">
i pg_header.inc
t <div style="padding:15px;">
t <h2 style="color:#12B5F5;">REGISTROS FLASH (LOGS)</h2>
c k7
t <div class="term">
t <table class="tb">
t <thead><tr>
t <th width="20%">MARCA TEMPORAL</th>
t <th width="15%">TIPO</th>
t <th width="65%">DESCRIPCION DEL EVENTO</th>
t </tr></thead>
t <tbody>
c z
t </tbody></table>
t </div>
t <form id="fb" action="logs.cgi" method="post">
t <input type="hidden" name="clear" value="YES">
t </form>
t <div style="margin-bottom:20px;">
t <input type="button" class="bt" value="ACTUALIZAR HISTORIAL" onclick="window.location.reload();">
t <input type="button" class="btr" value="BORRAR HISTORIAL" onclick="bd()">
t </div>
t <div style="border-top:1px solid #153B5A;padding-top:15px;">
t <a href="index.htm">&laquo; VOLVER</a>
t </div></div>
i pg_footer.inc
t </div></body></html>
.
