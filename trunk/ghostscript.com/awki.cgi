#!/usr/bin/awk -f
################################################################################
# awkiawki - wikiwiki clone written in (n|g|m)awk
# $Id: awki.cgi,v 1.45 2004/07/13 16:34:45 olt Exp $
################################################################################
# Copyright (c) 2002 Oliver Tonnhofer (olt@bogosoft.com)
# See the file `COPYING' for copyright notice.
################################################################################

BEGIN {
    #            --- default options ---
    #    --- use awki.conf to override default settings ---
    #
    #    img_tag: HTML img tag  for picture in page header.
    localconf["img_tag"] = "<img src=\"/awki.png\" width=\"48\" height=\"39\" align=\"left\">"
    #    datadir: Directory for raw pagedata (must be writeable for the script).
    localconf["datadir"] = "./data/"
    #    parser: Parsing script.
    localconf["parser"] = "./parser.awk"
    #   special_parser: Parser for special_* functions.
    localconf["special_parser"] = "./special_parser.awk"
    #    default_page: Name of the default_page.
    localconf["default_page"] = "FrontPage"
    #    show_changes: Number of changes listed by RecentChanges
    localconf["show_changes"] = 10
    #    max_post: Bytes accept by POST requests (to avoid DOS).
    localconf["max_post"] = 100000
    #    write_protection: Regex for write protected files
    #       e.g.: "*", "PageOne|PageTwo|^.*NonEditable" 
    #        HINT: to edit these protected pages, upload a .htaccess 
    #              protected awki.cgi script with write_protection = ""
    localconf["write_protection"] = ""
    #   css: HTTP URL for external CSS file.
    localconf["css"] = ""
    #   always_convert_spaces: If true, convert runs of 8 spaces to tab automatical.
    localconf["always_convert_spaces"] = 0
    #    date_cmd: Command for current date.
    localconf["date_cmd"] = "date '+%e %b. %G %R:%S %Z'"
    #    rcs: If true, rcs is used for revisioning.
    localconf["rcs"] = 0
    #    path: add path to PATH environment
    localconf["path"] = ""
    #            --- default options ---

    scriptname = ENVIRON["SCRIPT_NAME"]
    
    if (localconf["path"])
        ENVIRON["PATH"] = localconf["path"] ":" ENVIRON["PATH"]

    #load external configfile
    load_config(scriptname)
    
    # PATH_INFO contains page name
    if (ENVIRON["PATH_INFO"]) {    
        query["page"] = ENVIRON["PATH_INFO"]
    }
    
    if (ENVIRON["REQUEST_METHOD"] == "POST") {
        if (ENVIRON["CONTENT_TYPE"] == "application/x-www-form-urlencoded") {
            if (ENVIRON["CONTENT_LENGTH"] < localconf["max_post"])
                bytes = ENVIRON["CONTENT_LENGTH"]
            else
                bytes = localconf["max_post"]
            cmd = "dd ibs=1 count=" bytes " 2>/dev/null"
            cmd | getline query_str
            close (cmd)
        }
        if (ENVIRON["QUERY_STRING"]) 
            query_str = query_str "&" ENVIRON["QUERY_STRING"]
    } else {
        if (ENVIRON["QUERY_STRING"])    
            query_str = ENVIRON["QUERY_STRING"]
    }
    
    n = split(query_str, querys, "&")
    for (i=1; i<=n; i++) {
        split(querys[i], data, "=")
        query[data[1]] = data[2]
    }

    # (IMPORTANT for security!)
    query["page"] = clear_pagename(query["page"])
    query["revision"] = clear_revision(query["revision"])
    query["revision2"] = clear_revision(query["revision2"])
    query["string"] = clear_str(decode(query["string"]))
    
    if (!localconf["rcs"])
        query["revision"] = 0

    if (query["page"] == "")
        query["page"] = localconf["default_page"]
    
    query["filename"] = localconf["datadir"] query["page"]
    
    #check if page is editable
    special_pages = "Search|Index|Changes"
    
    if (query["page"] ~ "("special_pages")") {
        special_page = 1
    } else if (!localconf["write_protection"] || query["page"] !~ "("localconf["write_protection"]")") {
        page_editable = 1
    }


    header(query["page"])
    
    if (query["edit"] && page_editable)
        edit(query["page"], query["filename"], query["revision"])
    else if (query["save"] && query["text"] && page_editable)
        save(query["page"], query["text"], query["string"], query["filename"])
    else if (query["page"] ~ "Index")
        special_index(localconf["datadir"])
    else if (query["page"] ~ "Changes")
        special_changes(localconf["datadir"])
    else if (query["page"] ~ "Search")
        special_search(query["string"],localconf["datadir"])
    else if (query["page"] && query["history"])
        special_history(query["page"], query["filename"])
    else if (query["page"] && query["diff"] && query["revision"])
        special_diff(query["page"], query["filename"], query["revision"], query["revision2"])
    else 
        parse(query["page"], query["filename"], query["revision"])

    footer(query["page"])
    
}

# print header
function header(page) {
        pagename = page
        gsub(/_/, " ", pagename)

    print "Content-type: text/html\n"
    print "<html>\n<head>\n<title>Ghostscript: " pagename "</title>"
    if (localconf["css"])
        print "<link rel=\"stylesheet\" href=\"" localconf["css"] "\">"
    if (query["save"])
        print "<meta http-equiv=\"refresh\" content=\"2,URL="scriptname"/"page"\">"
    if (query["page"] ~ "Search")
	# protect robots from recursion
	print "<meta name=\"robots\" content=\"noindex,nofollow\" />"
    print "</head>\n<body>"
    print "<div class=head><h1>" localconf["img_tag"]
    print "<a href=\"" scriptname "/Search?string=" pagename "\">" pagename "</a>"
    print "</h1></div><div class=page>"
}

# print footer
function footer(page) {
    print "</div>"
    print "<div class=foot>"
    print "<table class=foot width=100%><tr><td class=footleft>"
    if (page_editable)
        # print "<a href=\""scriptname"?edit=true&amp;page="page"\">Edit "page"</a>"
        print "<a href=\""scriptname"?edit=true&amp;page="page"\">Edit</a>"
    #print "<a href=\""scriptname"/"localconf["default_page"]"\">"localconf["default_page"]"</a>"
    print "<a href=\""scriptname"/"localconf["default_page"]"\">Contents</a>"
    print "<a href=\""scriptname"/Index\">Index</a>"
    print "<a href=\""scriptname"/Changes\">Changes</a>"
    if (localconf["rcs"] && !special_page)
        print "<a href=\""scriptname"/"page"?history=true\">History</a>"
    print "<td class=footright>"
    print "<form action=\""scriptname"/Search\" method=\"GET\" align=\"right\">"
    print "Search: "
    print "<input type=\"text\" name=\"string\">"
    # print "<input type=\"submit\" value=\"Search\">"
    print "</form>\n"
    print "</table>"
    print "</div></body>\n</html>"
}

# send page to parser script
function parse(name, filename, revision) {
    if (system("[ -f "filename" ]") == 0 ) {
        if (revision) {
            print "<p><i>Displaying old version ("revision") of <a href=\""scriptname"/" name "\">"name"</a>.</i>"
            system("co -q -p'"revision"' " filename " | "localconf["parser"] " -v datadir='"localconf["datadir"] "'")
        } else
            system(localconf["parser"] " -v datadir='"localconf["datadir"] "' " filename)
    }
        else {
            print "<p><i>This page does not exist.</i>"
        }
}

function special_diff(page, filename, revision, revision2,   revisions) {
    if (system("[ -f "filename" ]") == 0) {
        print "<p><i>Displaying diff between "revision
        if (revision2)
            print " and "revision2
        else
            print " and current version"
        print " of <a href=\""scriptname"/"page "\">"page"</a>.</i>"
        if (revision2)
            revisions = "-r" revision " -r" revision2
        else
            revisions = "-r" revision
        system("rcsdiff "revisions" -u "filename" | " localconf["special_parser"] " -v special_diff='"page"'")
    }
}    

# print edit form
function edit(page, filename, revision,   cmd) {
        print "<p>"
    if (revision)
        print "<i>If you save previous versions, you'll overwrite the current page.</i>"
    print "<form action=\""scriptname"?save=true&amp;page="page"\" method=\"POST\">"
    print "<textarea name=\"text\" rows=30 cols=80>"
    # insert current page into textarea
    if (revision) {
        cmd = "co -q -p'"revision"' " filename
        while ((cmd | getline) >0)
            print
        close(cmd)
    } else {
        while ((getline < filename) >0)
            print
        close(filename)
    }
    print "</textarea><br />"
    if (localconf["rcs"])
        print "<p>Comment: <input type=\"text\" name=\"string\" maxlength=80 size=50>"
    print "<p><input type=\"submit\" value=\"Save\">"
#    if (! localconf["always_convert_spaces"])
#        print "<br>Convert runs of 8 spaces to Tab <input type=\"checkbox\" name=\"convertspaces\">"
    print "</form>"
print "<p><small><b>Fonts:</b> &lt;i&gt;<i>italic</i>&lt;/i&gt;; &lt;b&gt;<b>bold</b>&lt;/b&gt;; &lt;tt&gt;<tt>teletype</tt>&lt;/tt&gt;<br>\
<b>Heading:</b> = title <br>\
<b>Ruler:</b> ---- <br>\
<b>Formatted:</b> [space] indented text <br>\
<b>Lists:</b> * bullets; 1 numbered items; : term: definition <br>\
<b>Links:</b> [name of page], [url name of url] or raw url <br>\
</small>"
}

# save page
function save(page, text, comment, filename,   dtext, date) {
    dtext = decode(text);
    if ( localconf["always_convert_spaces"] || query["convertspaces"] == "on")
        gsub(/        /, "\t", dtext);
    ##### disable ##### print dtext > filename
    if (localconf["rcs"]) {
        localconf["date_cmd"] | getline date
        system("ci -q -t-"page" -l -m'"ENVIRON["REMOTE_ADDR"] ";;" date ";;"comment"' " filename)
    }
    print "<p>Saved <a href=\""scriptname"/"page"\">"page"</a>"
}

# list all pages
function special_index(datadir) {
        print "<p>"
    system("ls -1 " datadir " | " localconf["special_parser"] " -v special_index=yes")
    
}

# list pages with last modified time (sorted by date)
function special_changes(datadir,   date) {
    localconf["date_cmd"] | getline date
    print "<p>The current date is ", date "<p>"
    system("ls -tlL "datadir" | " localconf["special_parser"] " -v special_changes=" localconf["show_changes"])
}

function special_search(name,datadir) {
        print "<p>Search results for \"" name "\":<p>"
    system("grep -il '"name"' "datadir"* | " localconf["special_parser"] " -v special_search=yes")
}

function special_history(name, filename) {
    print "<p>Last changes on this page:<p>"
    system("rlog " filename " | sed -e s/----------------------------// | " localconf["special_parser"] " -v special_history="name)

    print "<p>Show diff between:"
    print "<form action=\""scriptname"/\" method=\"GET\">"
    print "<input type=\"hidden\" name=\"page\" value=\""name"\">"
    print "<input type=\"hidden\" name=\"diff\" value=\"true\">"
    print "<input type=\"text\" name=\"revision\" size=5>"
    print "<input type=\"text\" name=\"revision2\" size=5>"
    print "<input type=\"submit\" value=\"diff\">"
    print "</form></p>"
}

# remove '"` characters from string
# *** !Important for Security! ***
function clear_str(str) {
    gsub(/['`"]/, "", str)
    if (length(str) > 80) 
        return substr(str, 1, 80)
    else
        return str
}

# return the pagename
# *** !Important for Security! ***
function clear_pagename(str) {
    if (match(str, /[0-9A-Za-z_.,:-]+/))
        return substr(str, RSTART, RLENGTH)
    else
        return ""
}

# return revision numbers
# *** !Important for Security! ***
function clear_revision(str) {
    if (match(str, /[1-9]\.[0-9]+/))
        return substr(str, RSTART, RLENGTH)
    else
        return ""
}

# decode urlencoded string
function decode(text,   hex, i, hextab, decoded, len, c, c1, c2, code) {
    
    split("0 1 2 3 4 5 6 7 8 9 a b c d e f", hex, " ")
    for (i=0; i<16; i++) hextab[hex[i+1]] = i

    # urldecode function from Heiner Steven
    # http://www.shelldorado.com/scripts/cmds/urldecode

    # decode %xx to ASCII char 
    decoded = ""
    i = 1
    len = length(text)
    
    while ( i <= len ) {
        c = substr (text, i, 1)
        if ( c == "%" ) {
            if ( i+2 <= len ) {
                c1 = tolower(substr(text, i+1, 1))
                c2 = tolower(substr(text, i+2, 1))
                if ( hextab [c1] != "" || hextab [c2] != "" ) {
                    if ( (c1 >= 2 && c1 != 7) || (c1 == 7 && c2 != "f") || (c1 == 0 && c2 ~ "[9acd]") ){
                        code = 0 + hextab [c1] * 16 + hextab [c2] + 0
                        c = sprintf ("%c", code)
                    } else {
                        c = " "
                    }
                    i = i + 2
               }
            }
        } else if ( c == "+" ) {    # special handling: "+" means " "
            c = " "
        }
        decoded = decoded c
        ++i
    }
    
    # change linebreaks to \n
    gsub(/\r\n/, "\n", decoded)
    
    # remove last linebreak
    sub(/[\n\r]*$/,"",decoded)
    
    return decoded
}

#load configfile
function load_config(script,   configfile,key,value) {
    configfile = script
    #remove trailing / ('/awki/awki.cgi/' -> '/awki/awki.cgi')
    sub(/\/$/, "", configfile)
    #remove path ('/awki/awki.cgi' -> 'awki.cgi')
    sub(/^.*\//, "", configfile)
    #remove suffix ('awki.cgi' -> 'awki')
    sub(/\.[^.]*$/,"", configfile)
    # append .conf suffix
    configfile = configfile ".conf"
    
    #read configfile
    while((getline < configfile) >0) {
        if ($0 ~ /^#/) continue #ignore comments
        
        if (match($0,/[ \t]*=[ \t]*/)) {
            key = substr($0, 1, RSTART-1)
            sub(/^[ \t]*/, "", key)
            value = substr($0, RSTART+RLENGTH)
            sub(/[ \t]*$/, "", value)
            if (sub(/^"/, "", value))
                sub(/"$/, "", value) 
            #set localconf variables
            localconf[key] = value
        
        }
    }
}

