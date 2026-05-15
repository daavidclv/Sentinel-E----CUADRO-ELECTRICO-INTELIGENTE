/* Main function, that handles request and responses in background.
 * Response functions are handled if response code equals to OK 200. */
function updateMultiple(formUpd, callBack, userName, userPassword) {
 var xmlHttp = GetXmlHttpObject();
 if (xmlHttp == null) {
  alert("XmlHttp not initialized!");
  return 0;
 }

 xmlHttp.onreadystatechange = responseHandler;
 xmlHttp.open("GET", formUpd.url, true, userName, userPassword);
 xmlHttp.send(null);

 function responseHandler() {
  if (xmlHttp.readyState == 4) {
   if (xmlHttp.status == 200) {
    var xmlDoc = xmlHttp.responseXML;

    /* Fallback: algunos navegadores no rellenan responseXML */
    if (xmlDoc == null || xmlDoc.documentElement == null) {
     try {
      xmlDoc = (new DOMParser()).parseFromString(xmlHttp.responseText, "text/xml");
     }
     catch (e) {
      return 0;
     }
    }

    if (xmlDoc == null || xmlDoc.documentElement == null) {
     return 0;
    }

    try {
     processResponse(xmlDoc);
    }
    catch (e) {
     return 0;
    }

    if (callBack != undefined) {
     callBack();
    }
   }
   else if (xmlHttp.status == 401) {
    alert("Error code 401: Unauthorized");
   }
   else if (xmlHttp.status == 403) {
    alert("Error code 403: Forbidden");
   }
   else if (xmlHttp.status == 404) {
    alert("Error code 404: URL not found!");
   }
  }
 }
}

function processResponse(xmlDoc) {
  var textElementArr = xmlDoc.getElementsByTagName("text");
  for (var i = 0; i < textElementArr.length; i++) {
    try {
      var idNode = textElementArr[i].getElementsByTagName("id")[0];
      var valueNode = textElementArr[i].getElementsByTagName("value")[0];

      if (!idNode || !idNode.firstChild) {
        continue;
      }

      var elId = idNode.firstChild.nodeValue;
      var elValue = "";

      if (valueNode && valueNode.firstChild) {
        elValue = valueNode.firstChild.nodeValue;
      }

      document.getElementById(elId).value = elValue;
    }
    catch (error) {
    }
  }

  var checkboxElementArr = xmlDoc.getElementsByTagName("checkbox");
  for (var i = 0; i < checkboxElementArr.length; i++) {
    try {
      var idNode = checkboxElementArr[i].getElementsByTagName("id")[0];
      var onNode = checkboxElementArr[i].getElementsByTagName("on")[0];

      if (!idNode || !idNode.firstChild) {
        continue;
      }

      var elId = idNode.firstChild.nodeValue;
      var elValue = (onNode && onNode.firstChild) ? onNode.firstChild.nodeValue : "false";

      document.getElementById(elId).checked = elValue.match("true") ? true : false;
    }
    catch (error) {
    }
  }

  var selectElementArr = xmlDoc.getElementsByTagName("select");
  for (var i = 0; i < selectElementArr.length; i++) {
    try {
      var idNode = selectElementArr[i].getElementsByTagName("id")[0];
      var valueNode = selectElementArr[i].getElementsByTagName("value")[0];

      if (!idNode || !idNode.firstChild) {
        continue;
      }

      var elId = idNode.firstChild.nodeValue;
      var elValue = (valueNode && valueNode.firstChild) ? valueNode.firstChild.nodeValue : "";

      document.getElementById(elId).value = elValue;
    }
    catch (error) {
    }
  }

  var radioElementArr = xmlDoc.getElementsByTagName("radio");
  for (var i = 0; i < radioElementArr.length; i++) {
    try {
      var idNode = radioElementArr[i].getElementsByTagName("id")[0];
      var onNode = radioElementArr[i].getElementsByTagName("on")[0];

      if (!idNode || !idNode.firstChild) {
        continue;
      }

      var elId = idNode.firstChild.nodeValue;
      var elValue = (onNode && onNode.firstChild) ? onNode.firstChild.nodeValue : "false";

      document.getElementById(elId).checked = elValue.match("true") ? true : false;
    }
    catch (error) {
    }
  }
}
  
/* XMLHttpRequest object specific functions */
function GetXmlHttpObject() { //init XMLHttp object
 var xmlHttp=null;
 try {
  xmlHttp=new XMLHttpRequest(); // Firefox, Opera 8.0+, Safari
 }
 catch (e) {
  try {   // Internet Explorer
   xmlHttp=new ActiveXObject("Msxml2.XMLHTTP");
  }
  catch (e) {
   xmlHttp=new ActiveXObject("Microsoft.XMLHTTP");
  }
 }
 return xmlHttp;
}

/* Objects templates */
function periodicObj(url, period) {
 this.url = url;
 this.period = (typeof period == "undefined") ? 0 : period;
}
