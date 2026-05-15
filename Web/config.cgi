t <html><head><title>SENTINEL-E | Configuracion</title>
t <script>
t function sv(f){
t   if(confirm('\xbfGuardar nueva configuracion?'))f.submit();
t }
t </script>
t <style>
t body,table,td,tr,th{background:#050B19!important;color:#E7F1F8!important;}
t body{margin:0;font-family:Verdana;text-align:center;}
t .cnt{width:900px;border:1px solid #153B5A;margin:20px auto;}
t .bx{background:#081425;border:1px solid #234D70;padding:20px;margin:20px;}
t .hd{color:#12B5F5;font-size:13px;border-bottom:1px solid #16A7D9;}
t .hd{padding-bottom:5px;text-align:left;font-weight:bold;}
t .tb{width:100%;border-collapse:collapse;margin-top:10px;}
t .tb td{padding:10px 5px;text-align:left;font-size:12px;}
t .tb td{border-bottom:1px dashed #153B5A;}
t .in{background:#02050A;color:#43D7FF;border:1px solid #16A7D9;}
t .in{padding:6px;width:95%;}
t .in:focus{outline:none;border-color:#FFF;color:#FFF;}
t .bt{background:#12B5F5;color:#050B19;border:none;padding:10px 20px;}
t .bt{font-weight:bold;cursor:pointer;letter-spacing:1px;}
t a{color:#43D7FF;text-decoration:none;font-size:12px;}
t #cont{position:fixed;bottom:10px;right:15px;font-size:12px;color:#888;}
t </style></head>
t <body><div class="cnt">
i pg_header.inc
t <div style="padding:15px;">
t <h2 style="color:#12B5F5;">CONFIGURACION DE SISTEMA</h2>
t <form action="config.cgi" method="post">
t <input type="hidden" name="pg" value="cfg">
t <div class="bx">
t <div class="hd">PARAMETROS DE FUNCIONAMIENTO</div>
t <table class="tb">
t <tr><td width=50%>ID Dispositivo (Texto libre)</td>
t <td width=50%>
c k 1 
t </td></tr>
t <tr><td>Ubicacion / Zona</td>
t <td>
c k 2 
t </td></tr>
t </table></div>
t <div class="bx">
t <div class="hd">AJUSTES DE MUESTREO Y ALARMAS</div>
t <table class="tb">
t <tr><td width=50%>Tasa de refresco datos (seg)</td>
t <td width=50%>
c k 3 
t </td></tr>
t <tr><td>Umbral Alarma Temp. Max (&deg;C)</td>
t <td>
c k 4 
t </td></tr>
t <tr><td>Umbral Alarma Corriente Max (A)</td>
t <td>
c k 5 
t </td></tr>
t </table></div>
t <div class="bx" style="border-color:#12B5F5;background:#0A1730;">
t <div class="hd" style="color:#FFF;">ADMINISTRACION RFID</div>
t <table class="tb">
t <tr><td width=50%>Nuevo UID Master (Hexadecimal)</td>
t <td width=50%>
c k 6 
t </td></tr>
t </table></div>
t <p id="est" style="margin:30px 0;">
c k 7 
t </p>
t <p style="margin:30px 0;">
t <input type="button" class="bt" value="GUARDAR CONFIGURACION" onclick="sv(this.form)">
t &nbsp;&nbsp;
t <input type="reset" value="CANCELAR" style="background:none;color:#43D7FF;border:none;">
t </p></form>
t <div style="border-top:1px solid #153B5A;padding-top:15px;">
t <a href="index.htm">&laquo; VOLVER</a>
t </div></div>
i pg_footer.inc
t </div>
t <div id="cont"></div>
t <script>
t (function(){
t var pe=document.getElementById('est');
t var pc=document.getElementById('cont');
t function ref(){
t var x=new XMLHttpRequest();
t x.open('GET','status.cgi?t='+Date.now(),true);
t x.onreadystatechange=function(){
t if(x.readyState==4&&x.status==200){
t try{var d=JSON.parse(x.responseText);
t if(d.u){
t pe.innerHTML='<b style="color:#0F0">DESBLOQUEADO</b>';
t pc.textContent='Tiempo restante: '+d.s+' s';
t var i=document.querySelectorAll('.in');
t for(var k=0;k<i.length;k++){if(i[k].disabled) i[k].disabled=false;}
t }else{
t pe.innerHTML='<b style="color:#F00">BLOQUEADO: Pase Tarjeta</b>';
t pc.textContent='';
t var i=document.querySelectorAll('.in');
t for(var k=0;k<i.length;k++){if(!i[k].disabled) i[k].disabled=true;}
t }
t }catch(e){}
t }
t };
t x.send();
t }
t ref();setInterval(ref,1000);
t })();
t </script>
t </body></html>
.