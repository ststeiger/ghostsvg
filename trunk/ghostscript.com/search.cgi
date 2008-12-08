#!/usr/bin/awk -f
################################################################################
# awkiawki - wikiwiki clone written in (n|g|m)awk
# $Id: awki.cgi,v 1.45 2004/07/13 16:34:45 olt Exp $
################################################################################
# Copyright (c) 2002 Oliver Tonnhofer (olt@bogosoft.com)
# Copyright 2008 Artifex Software, Inc.
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
    load_config("awki.conf")
    
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

    print "Content-type: text-html\n"

    special_search(query["string"])
}

# return search results
function special_search(name) {
    page = "Search results for '" name "':\n"
    cmd = "grep -l '" name "' *.awki"
    listpage(page, cmd, "Ghostscript Search")
}

# list all pages
function special_index() {
    page = "Available pages:\n"
    cmd = "ls -1 *.awki"
    listpage(page, cmd, "Site Index")
}

# create an awki page from a list and pass it to the parser to create html
function listpage(page, cmd, title) {
    while ((cmd | getline) > 0) {
        gsub(/\.awki/, "")
        gsub(/_/, " ")
        gsub(/.*/, "* [&]")
        page = page $_ "\n"
    }
    close(cmd)
    system("echo '" page "' | awk -f parser.awk -v FILENAME='" title "'")
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
    sub(/[\n\r]*$/, "", decoded)
    
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
