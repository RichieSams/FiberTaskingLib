<!DOCTYPE html>
<html>
 <head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
  <title>{$title} - nonius report</title>
  <style type="text/css"> {% include "report.css" %} </style>
  <script type="text/javascript"> {% include "lib/plotly/plotly-basic.min.js" %} </script>
 </head>
 <body>
   <div id="header">
     <label>plot:</label>
     <div class="select">
       <span class="arr"></span>
       <select id="chooser">
         <option value="summary">summary</option>
         {% for run in runs %}
         <option value="{$loop.index0}">samples
           {% for param in run.params %}
           | {$param.name}={$param.value}
           {% endfor %}
         </option>
         {% endfor %}
       </select>
     </div>
   </div>
   <div id="plot"></div>
   <div id="footer">Generated with <a href="http://flamingdangerzone.com/nonius">nonius</a></div>
   <script type="text/javascript"> {% include "report.tpl.js" %} </script>
 </body>
</html>
