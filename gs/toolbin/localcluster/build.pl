#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;
use POSIX ":sys_wait_h";


my $updateTestFiles=1;

my $verbose=0;

local $| = 1;

my $baseDirectory;
$baseDirectory='./';

my $temp="./temp";
#$temp="/tmp/space/temp";
#$temp="/dev/shm/temp";

my $gsBin=$baseDirectory."gs/bin/gs";
my $pclBin=$baseDirectory."ghostpdl/main/obj/pcl6";
my $xpsBin=$baseDirectory."ghostpdl/xps/obj/gxps";
my $svgBin=$baseDirectory."ghostpdl/svg/obj/gsvg";

my %testSource=(
  $baseDirectory."tests/pdf" => 'gs',
  $baseDirectory."tests/ps" => 'gs',
  $baseDirectory."tests/eps" => 'gs',
  $baseDirectory."tests_private/ps/ps3cet" => 'gs',
  $baseDirectory."tests_private/comparefiles" => 'gs',
  $baseDirectory."tests_private/pdf/PDFIA1.7_SUBSET" => 'gs',
  $baseDirectory."tests/pcl" => 'pcl',
  $baseDirectory."tests_private/pcl/pcl5cfts" => 'pcl',
  $baseDirectory."tests_private/pcl/pcl5efts" => 'pcl',
  $baseDirectory."tests_private/xl/pxlfts3.0" => 'pcl',
# $baseDirectory."tests/xps" => 'xps',
# $baseDirectory."tests_private/xps/xpsfts-a4" => 'xps',
# $baseDirectory."tests/svg/svgw3c-1.1-full/svg" => 'svg',
# $baseDirectory."tests/svg/svgw3c-1.1-full/svgHarness" => 'svg',
# $baseDirectory."tests/svg/svgw3c-1.1-full/svggen" => 'svg',
# $baseDirectory."tests/svg/svgw3c-1.2-tiny/svg" => 'svg',
# $baseDirectory."tests/svg/svgw3c-1.2-tiny/svgHarness" => 'svg',
# $baseDirectory."tests/svg/svgw3c-1.2-tiny/svggen" => 'svg'
);


my $cmd;
my $s;
my $previousRev;
my $newRev=99999;

my %tests=(
'gs' => [
"pbmraw.72.0",
"pbmraw.300.0",
"pbmraw.300.1",
"pgmraw.72.0",
"pgmraw.300.0",
"pgmraw.300.1",
"pkmraw.72.0",
"pkmraw.300.0",
"pkmraw.300.1",
"ppmraw.72.0",
"ppmraw.300.0",
"ppmraw.300.1",
#"psdcmyk.72.0",
##"psdcmyk.300.0",
##"psdcmyk.300.1",
"pdf.ppmraw.72.0",
"pdf.ppmraw.300.0",
"pdf.pkmraw.300.0"
],
'pcl' => [
"pbmraw.75.0",
"pbmraw.600.0",
"pbmraw.600.1",
"pgmraw.75.0",
"pgmraw.600.0",
"pgmraw.600.1",
#"wtsimdi.75.0",
#"wtsimdi.600.0",
#"wtsimdi.600.1",
"ppmraw.75.0",
"ppmraw.600.0",
"ppmraw.600.1",
"bitrgb.75.0",
"bitrgb.600.0",
"bitrgb.600.1",
#"psdcmyk.75.0",
##"psdcmyk.600.0",
##"psdcmyk.600.1",
"pdf.ppmraw.75.0",
"pdf.ppmraw.600.0",
"pdf.pkmraw.600.0"
],
'xps' => [
"pbmraw.72.0",
"pbmraw.300.0",
"pbmraw.300.1",
"pgmraw.72.0",
"pgmraw.300.0",
"pgmraw.300.1",
"wtsimdi.72.0",
"wtsimdi.300.0",
"wtsimdi.300.1",
"ppmraw.72.0",
"ppmraw.300.0",
"ppmraw.300.1",
"bitrgb.72.0",
"bitrgb.300.0",
"bitrgb.300.1",
#"psdcmyk.72.0",
##"psdcmyk.300.0",
##"psdcmyk.300.1",
"pdf.ppmraw.72.0",
"pdf.ppmraw.300.0",
"pdf.pkmraw.300.0"
],
'svg' => [
"pbmraw.72.0",
"pbmraw.300.0",
"pbmraw.300.1",
"pgmraw.72.0",
"pgmraw.300.0",
"pgmraw.300.1",
"wtsimdi.72.0",
"wtsimdi.300.0",
"wtsimdi.300.1",
"ppmraw.72.0",
"ppmraw.300.0",
"ppmraw.300.1",
"bitrgb.72.0",
"bitrgb.300.0",
"bitrgb.300.1",
#"psdcmyk.72.0",
##"psdcmyk.300.0",
##"psdcmyk.300.1",
"pdf.ppmraw.72.0",
"pdf.ppmraw.300.0",
"pdf.pkmraw.300.0"
]
);


#update the regression file source directories
if ($updateTestFiles) {

  foreach my $testSource (sort keys %testSource) {
  $cmd="cd $testSource ; /usr/local/bin/svn update";
  print STDERR "$cmd\n" if ($verbose);
  `$cmd`;
  }

}



# build a list of the source files
my %testfiles;
foreach my $testSource (sort keys %testSource) {
  opendir(DIR, $testSource) || die "can't opendir $testSource: $!";
  foreach (readdir(DIR)) {
    $testfiles{$testSource.'/'.$_}=$testSource{$testSource} if (!-d $testSource.'/'.$_ && ! m/^\./ && ! m/.disabled$/);
  }
  closedir DIR;
}

#print Dumper(\%testfiles);

#exit;

sub build($$$$) {
  my $product=shift;  # gs|pcl|xps|svg
  my $inputFilename=shift;
  my $options=shift;
  my $md5sumOnly=shift;

  my $cmd="";
  my $cmd1="";
  my $cmd2="";
  my $outputFilenames="";
  my $rmCmd="true";

  my @a=split '\.',$options;

  my $filename=$inputFilename;
  $filename =~ s|.+/||;

  my $tempname=$inputFilename;
  $tempname =~ s|^./||;
  $tempname =~ s|/|__|g;
  my $logFilename="$temp/$tempname.$options.log";
  my $md5Filename="$temp/$tempname.$options.md5";
  my $filename2="$tempname.$options";

  my $outputFilename;

# $cmd .= " touch $logFilename ; rm -f $logFilename ";

  $cmd  .= " true";

  $cmd .= " ; echo \"$product\" >>$logFilename ";

  if ($a[0] eq 'pdf') {

    $outputFilename="$temp/$tempname.$options.pdf";
    if ($product eq 'gs') {
      $cmd1.=" $gsBin";
    } elsif ($product eq 'pcl') {
      $cmd1.=" $pclBin";
    } elsif ($product eq 'xps') {
      $cmd1.=" $xpsBin";
    } elsif ($product eq 'svg') {
      $cmd1.=" $svgBin";
    } else {
      die "unexpected product: $product";
    }
    $cmd1.=" -sOutputFile=$outputFilename";
    $cmd1.=" -sDEVICE=pdfwrite";
#   $cmd1.=" -q" if ($product eq 'gs');
    $cmd1.=" -sDEFAULTPAPERSIZE=letter" if ($product eq 'gs');
    $cmd1.=" -dNOPAUSE -dBATCH";  # -Z:
    $cmd1.=" -dNOOUTERSAVE -dJOBSERVER -c false 0 startjob pop -f" if ($product eq 'gs');

    $cmd1.=" %rom%Resource/Init/gs_cet.ps" if ($filename =~ m/.PS$/ && $product eq 'gs');
#   $cmd1.=" -dFirstPage=1 -dLastPage=1" if ($filename =~ m/.pdf$/i || $filename =~ m/.ai$/i);

    $cmd1.=" - < " if (!($filename =~ m/.pdf$/i || $filename =~ m/.ai$/i) && $product eq 'gs');

    $cmd1.=" $inputFilename";
#   $cmd.=" 2>&1";
    
$cmd.=" ; echo \"$cmd1\" >>$logFilename ";
    $cmd.=" ; $cmd1 >>$logFilename 2>&1";

    $cmd.=" ; echo '---' >>$logFilename";

    my $inputFilename=$outputFilename;
    $outputFilename="$temp/$filename.$options";

    $cmd2.=" $gsBin";
    if ($md5sumOnly) {
      $cmd2.=" -sOutputFile='|md5sum >>$md5Filename'";
    } else {
      $cmd2.=" -sOutputFile=$outputFilename";
    }

    $cmd2.=" -dMaxBitmap=30000000" if ($a[3]==0);
    $cmd2.=" -dMaxBitmap=10000"    if ($a[3]==1);

    $cmd2.=" -sDEVICE=".$a[1];
    $cmd2.=" -dGrayValues=256" if ($a[0] eq 'bitrgb');
    $cmd2.=" -r".$a[2];
#   $cmd2.=" -q"
    $cmd2.=" -sDEFAULTPAPERSIZE=letter" if ($product eq 'gs');
    $cmd2.=" -dNOPAUSE -dBATCH -K1000000";  # -Z:
    $cmd2.=" -dNOOUTERSAVE -dJOBSERVER -c false 0 startjob pop -f";

#   $cmd2.=" -dFirstPage=1 -dLastPage=1";

    $cmd2.=" $inputFilename";

$cmd.=" ; echo \"$cmd2\" >>$logFilename ";
#   $cmd.=" ; $timeCommand -f \"%U %S %E %P\" $cmd2 >>$logFilename 2>&1";
    $cmd.=" ;  $cmd2 >>$logFilename 2>&1";

#   $cmd.=" ; gzip -f $inputFilename >>$logFilename 2>&1";
    $outputFilenames.="$inputFilename ";

  } else {
    $outputFilename="$temp/$filename.$options";
    if ($product eq 'gs') {
      $cmd2.=" $gsBin";
    } elsif ($product eq 'pcl') {
      $cmd2.=" $pclBin";
    } elsif ($product eq 'xps') {
      $cmd2.=" $xpsBin";
    } elsif ($product eq 'svg') {
      $cmd2.=" $svgBin";
    } else {
      die "unexpected product: $product";
    }
    if ($md5sumOnly) {
      $cmd2.=" -sOutputFile='|md5sum  >>$md5Filename'";
    } else {
      $cmd2.=" -sOutputFile=$outputFilename";
    }

    $cmd2.=" -dMaxBitmap=30000000" if ($a[2]==0);
    $cmd2.=" -dMaxBitmap=10000"    if ($a[2]==1);

    $cmd2.=" -sDEVICE=".$a[0];
    $cmd2.=" -dGrayValues=256" if ($a[0] eq 'bitrgb');
    $cmd2.=" -r".$a[1];
#   $cmd2.=" -q" if ($product eq 'gs');
    $cmd2.=" -sDEFAULTPAPERSIZE=letter" if ($product eq 'gs');
    $cmd2.=" -dNOPAUSE -dBATCH -K1000000";  # -Z:
    $cmd2.=" -dNOOUTERSAVE -dJOBSERVER -c false 0 startjob pop -f" if ($product eq 'gs');

    $cmd2.=" %rom%Resource/Init/gs_cet.ps" if ($filename =~ m/.PS$/ && $product eq 'gs');
#   $cmd2.=" -dFirstPage=1 -dLastPage=1" if ($filename =~ m/.pdf$/i || $filename =~ m/.ai$/i);

    $cmd2.=" - < " if (!($filename =~ m/.pdf$/i || $filename =~ m/.ai$/i) && $product eq 'gs');

    $cmd2.=" $inputFilename ";

$cmd.=" ; echo \"$cmd2\" >>$logFilename ";
#   $cmd.=" ; $timeCommand -f \"%U %S %E %P\" $cmd2 >>$logFilename 2>&1";
    $cmd.=" ; $cmd2 >>$logFilename 2>&1";


  }
  if ($md5sumOnly) {
  } else {
    $cmd.=" ; md5sum $outputFilename >>$md5Filename 2>&1 ";
    $outputFilenames.="$outputFilename";
  }

# $cmd.=" ; gzip -f $outputFilename >>$logFilename 2>&1 ";

  

  return($cmd,$outputFilenames,$filename2);
}

  


my @commands;
my @filenames;


foreach my $testfile (sort keys %testfiles) {
  foreach my $test (@{$tests{$testfiles{$testfile}}}) {
# foreach my $test (@{$tests{'gs'}}) {
    my $t=$testfile.".".$test;
      my $cmd="";
      my $outputFilenames="";
      my $filename="";
      ($cmd,$outputFilenames,$filename)=build($testfiles{$testfile},$testfile,$test,1);
      push @commands,$cmd;
      push @filenames,$filename;
  }
}



while (scalar(@commands)) {
  my $n=rand(scalar @commands);
  my $command=$commands[$n];  splice(@commands,$n,1);
  my $filename=$filenames[$n];  splice(@filenames,$n,1);
  print "$filename\t$command\n";
# print "$command\n";
}

