The only web page that should be in this directory is index.htm.
Files that were in http://www.cs.wisc.edu/~ghost/ are now
in the doc subdirectory.
This allows http://www.ghostscript.com/ and http://www.cs.wisc.edu/~ghost/
to have identical copies, each updated from the CVS master copy.

Pages should use the style sheet doc/gsweb.css, except for
doc/AFPL/N.NN/* which use their own gs.css.

Where possible, please use short 8.3 filenames so that these 
pages can also be accessed when written on an ISO9660 CD.
Within the website, link to the index.htm file in a 
directory where possible so that the links work when
the pages are on a hard disk or CD.

The doc/ tree is generally maintained by checking out the
new files from cvs by their release tag. Versions from before
ghostscript was in cvs are kept explicitly in the website module.

Russell Lang
2001-03-29
