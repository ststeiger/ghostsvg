#!/usr/bin/awk -f
################################################################################
# parser.awk - parsing script for awkiawki
# $Id: parser.awk,v 1.6 2002/12/07 13:46:45 olt Exp $
################################################################################
# Copyright (c) 2002 Oliver Tonnhofer (olt@bogosoft.com)
# Copyright 2008 Artifex Sofware, Inc.
# See the file `COPYING' for copyright notice.
################################################################################
#
# Changed almost everything. -- Tor Andersson
# Merged header and footer, switched to static pages.  -- Ralph Giles
#

BEGIN {
    #    --- default config ---
    #    img_tag: HTML img tag  for picture in page header.
    localconf["img_tag"] = "<img src=\"logo.png\" width=\"56\" height=\"64\" align=\"right\">"
    #    css: Stylesheet to use
    localconf["css"] = "style.css"

    list["ol"] = 0
    list["ul"] = 0
    list["dl"] = 0
    FS = "[ ]"
    
    cmd = "ls *.awki"
    while ((cmd | getline ls_out) >0)
        if (match(ls_out, /[0-9A-Za-z_.,:-]+\.awki/) && 
            substr(ls_out, RSTART + RLENGTH) !~ /,v/) {
            page = substr(ls_out, RSTART, RLENGTH - 5)
            pages[tolower(page)] = page
        }
    close(cmd)

    # for some reason we don't get FILENAME set correctly
    if (!FILENAME) FILENAME = ARGV[ARGC - 1]
    page = FILENAME
    sub(/\..*$/, "", page)
    header(page)
}

# register blanklines
/^$/ { blankline = 1; next; }

# HTML entities for <, > and & except <b> and <i>
/[&<>]/ {
    gsub(/<b>/, "[b]"); gsub(/<\/b>/, "[/b]");
    gsub(/<i>/, "[i]"); gsub(/<\/i>/, "[/i]");
    gsub(/<tt>/, "[tt]"); gsub(/<\/tt>/, "[/tt]");

    gsub(/&/, "\\&amp;");
    gsub(/</, "\\&lt;");
    gsub(/>/, "\\&gt;");

    gsub(/\[b\]/, "<b>"); gsub(/\[\/b\]/, "</b>");
    gsub(/\[i\]/, "<i>"); gsub(/\[\/i\]/, "</i>");
    gsub(/\[tt\]/, "<tt>"); gsub(/\[\/tt\]/, "</tt>");
}

# Generate links to http://image/foo.jpeg, http://urls/ and named urls [http://foo/ name of the link]

/\[?(https?|ftp|mailto):/ {
    workbuf = ""
    postbuf = $0
    while (match(postbuf, /(\[(https?|ftp|mailto):[^\]]*\]|(https?|ftp|mailto):[^ ]*)/) > 0)
    {
        initbuf = substr(postbuf, 1, RSTART - 1)
        linkbuf = substr(postbuf, RSTART, RLENGTH)
        postbuf = substr(postbuf, RSTART + RLENGTH, length(postbuf))

        if (linkbuf ~ /\[(https?|ftp|mailto):[^ ]*/) {
            sub(/\[(https?|ftp|mailto):[^ ]*/, "<a href=\"&\">", linkbuf)
            sub(/href="\[/, "href=\"", linkbuf)
            sub(/\]/, "</a>", linkbuf)
        }
        else if (linkbuf ~ /http:[^ ]*\.(jpg|jpeg|png|gif)/) {
            sub(/http:[^ ]*\.(jpg|jpeg|png|gif)/, "<img src=\"&\">", linkbuf)
        }
        else if (linkbuf ~ /(https?|ftp|mailto):[^ ]*/) {
            sub(/(https?|ftp|mailto):[^ ]*/, "<a href=\"&\">&</a>", linkbuf)
        }

        workbuf = workbuf initbuf linkbuf
    }
    $0 = workbuf postbuf
}

# Generate links to pages

/\[[0-9A-Za-z .,:-]+\]/ && ! /^ / {
    workbuf = ""
    postbuf = $0
    while (match(postbuf, /\[[0-9A-Za-z .,:-]+\]/))
    {
        initbuf = substr(postbuf, 1, RSTART - 1)
        linkbuf = substr(postbuf, RSTART, RLENGTH)
        postbuf = substr(postbuf, RSTART + RLENGTH, length(postbuf))
        
        pagename = substr(linkbuf, 2, length(linkbuf) - 2)
        filename = pagename
        gsub(/ /, "_", filename)

        if (pages[tolower(filename)])
        {
            filename = pages[tolower(filename)]
            linkbuf = "<a href=\"" filename ".html\">" pagename "</a>"
        }
        else
        {
            linkbuf = "<a class=nowhere href=\"" filename "\">" pagename "</a>"
        }

        workbuf = workbuf initbuf linkbuf
    }
    $0 = workbuf postbuf
}

# Generate links to bug numbers

/#[0-9]+/ {
    gsub(/#[0-9]+/, "<a href=\"http://bugs.ghostscript.com/show_bug.cgi?id=&\">&</a>")
    gsub(/id=#/, "id=")
}

/^= / { $0 = "<h2>" substr($0, 2) "</h2>"; close_tags(); print; next; }

/^---/ { sub(/^---+/, "<hr>"); blankline = 1; close_tags(); print; next; }

/^[*] / { close_tags("list"); parse_list("ul", "ol", "dl"); print; next;}
/^[1] / { close_tags("list"); parse_list("ol", "ul", "dl"); print; next;}
/^[:] / { close_tags("list"); parse_list("dl", "ul", "ol"); print; next;}

/^ / { 
    close_tags("pre");
    if (pre != 1) {
        print "<pre class=code>\n" $0; pre = 1
        blankline = 0
    } else { 
        if (blankline==1) {
            print ""; blankline = 0
        }
        print;
    }
    next;
}

NR == 1 { print "<p>"; }
{
    close_tags();
    
    # print paragraph when blankline registered
    if (blankline==1) {
        print "<p>";
        blankline=0;
    }

    #print line
    print;

}

END {
    $0 = ""
    close_tags()
    footer(page)
}

function close_tags(not) {

    # if list is parsed this line print it
    if (not !~ "list") {
        if (list["ol"] > 0) {
            parse_list("ol", "ul", "dl")
        } else if (list["ul"] > 0) {
            parse_list("ul", "ol", "dl")
        } else if (list["dl"] > 0) {
            parse_list("dl", "ul", "ol")
        } 
    }

    # close monospace
    if (not !~ "pre") {
        if (pre == 1) {
            print "</pre>"
                pre = 0
        }
    }

}

function parse_list(this, other1, other2) {

    thislist = list[this]
    other1list = list[other1]
    other2list = list[other2]
    tabcount = 0

    while (/^[1*:]+ /) {
        sub(/^[1*:]/, "")
        tabcount++
    }

    # close foreign tags
    if (other1list > 0) {
        while(other1list-- > 0) {
            print "</" other1 ">"
        }
    }
    if (other2list > 0) {
        while(other2list-- > 0) {
            print "</" other2 ">"
        }
    }

    # if we are needing more tags we open new
    if (thislist < tabcount) {
        while(thislist++ < tabcount) {
            print "<" this ">"
        }
    }
    # if we are needing less tags we close some
    else if (thislist > tabcount) {
        while(thislist-- != tabcount) {
            print "</" this ">"
        }
    }

    if (tabcount) {
        if (this == "dl") {
            sub(/:/, ":<dd>")
            $0 = " <dt>" $0
        }
        else {
            $0 = " <li>" $0 "</li>"
        }
    }

    list[other1] = 0
    list[other2] = 0
    list[this] = tabcount
}

# print header
function header(page) {
    pagename = page
    gsub(/_/, " ", pagename)
    if (pagename ~ "index")
        pagename = "Ghostscript"
    
    print "<html>\n<head>\n<title>Ghostscript: " pagename "</title>"
    if (localconf["css"])
        print "<link rel=\"stylesheet\" href=\"" localconf["css"] "\">"
    if (query["page"] ~ "Search")
        # protect robots from recursion
        print "<meta name=\"robots\" content=\"noindex,nofollow\" />"
    print "</head>\n<body>"
    print "<div class=head><h1>"
    if (localconf["img_tag"])
        print localconf["img_tag"]
    print pagename
    print "</h1></div><div class=page>"
}

# print footer
function footer(page) {
    print "</div>"
    print "<div class=foot>"
    print "<table class=foot width=100%><tr><td class=footleft>"
    print "<a href=\"/\">Home</a>"
    print "<a href=\"contents.html\">Contents</a>"
    print "<td class=footright>"
    print "<form action=\"/search\" method=\"GET\" align=\"right\">"
    print "Search: "
    print "<input type=\"text\" name=\"string\">"
    # print "<input type=\"submit\" value=\"search\">"
    print "</form>\n"
    print "</table>"
    print "</div></body>\n</html>"
}

