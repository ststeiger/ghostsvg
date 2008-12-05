/^<!-- \[1.1 begin headline/ a\
<div class=head>
/^<!-- \[1.1 end headline/ a\
</div><div class=page>
/^<!-- \[3.0 begin visible trailer/ a\
</div><div class=foot>
/^<!-- \[3.0 end visible trailer/ a\
</div>
/^<hr>$/ d
/bgcolor="#CCCC00"><hr><font size="+1">/s/bgcolor="#CCCC00"><hr><font size="+1">/class=fancy>/;s/<\/font><hr>//
s/#CCCC00/#DDD/
s!<blockquote><table !<table class=fancy !
s!</table></blockquote>!</table>!
s!<blockquote><ul>!<ul>!
s!</ul></blockquote>!</ul>!
s!href="..\/!href="http://svn.ghostscript.com:8080/ghostscript/trunk/gs/!g
