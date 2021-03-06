#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;
use POSIX ":sys_wait_h";

my $file="";
my $error=0;
my $md5sum=0;
my $divider=0;

my $t1;
my $t2;
my $t3;

my %results;

my $input=shift;
my $output=shift;
my $machine=shift;
my $input2=shift;
my $rev=shift;

# from compare.pl:
# 0: "none",
# 1: "Error_reading_input_file",
# 2: "Error_reading_Ghostscript_produced_PDF/PS_file",
# 3: "Timeout_reading_input_file",
# 4: "Timeout_reading_Ghostscript_produced_PDF/PS_File",
# 5: "Input_file_missing",
# 6: "Ghostscript_generated_PDF/PS_file_missing",
# 7: "Seg_Fault_during_pdfwrite",
# 8: "Seg_Fault",
# 9: "Seg_Fault_reading_Ghostscript_produced_PDF/PS_File",
# 10: "Internal_error"


$rev=0 if (!$rev);

($machine) || die "usage: readlog.pl input.log output machine [input.out] [rev]";

open (F,"<$input") || die "file $input not found";

while(<F>) {

  chomp;

  if (m/^compileFail/ || m/^md5sumFail/ || m/^timeoutFail/) {
    close(F);
    print "$_\n";
    exit;
  }
  if (m/===gs_build===/) {
    open (F2,">gs_build.log");
    while(<F>) {
      chomp;
      close(F2) if (m/^===(.+)===$/);
      last if (m/^===(.+)===$/);
      print F2 "$_\n";
    }
  }
  if ($_) {

  if ((m/===(.+).log===/ || m/===(.+)===/) && !m/=====/) {
    $file=$1;
    $error=0;
    $divider=0;
    $md5sum=0;
    $results{$file}{"error"}=-1;
    $results{$file}{"md5"}=$md5sum;
    $results{$file}{"time1"}=0;
    $results{$file}{"time2"}=0;
    $results{$file}{"time3"}=0;
    $results{$file}{"time4"}=0;
    $t1=0;
    $t2=0;
    $t3=0;
    my $t=<F>;
    chomp $t;
    $results{$file}{"product"}=$t;
  }
  if (m/^---$/) {
    $divider=1;
  }

  if (m/Unrecoverable error, exit code/ || m/Command exited with non-zero status/ || m/Warning interpreter exited with error code/) {
    $error=1 if ($divider==0 && $error==0);
    $error=2 if ($divider==1 && $error==0);
    $results{$file}{"error"}=$error;
  }
  if (m/Segmentation fault/ || m/Backtrace:/ || m/Command terminated by signal/) {
    $error=8 if ($divider==0 && $error==0);
    $error=9 if ($divider==1 && $error==0);
    $results{$file}{"error"}=$error;
  }
  if (m/killed: timeout/) {
    $error=3 if ($divider==0); # && $error==0);
    $error=4 if ($divider==1); # && $error==0);
    $results{$file}{"error"}=$error;
  }
  if (m/Unable to open/) {
    $error=5 if ($divider==0 && $error==0);
    $error=6 if ($divider==1 && $error==0);
    $results{$file}{"error"}=$error;
  }
  if (m/(\d+\.\d+) (\d+\.\d+) (\d+:\d\d\.\d\d) (\d+)%/) {
    if ($t1==0) {
      $t1=$1;
    } elsif ($t2==0) {
      $t2=$1;
    } else {
      $t3=$1;
    }
#   print "time=$t1 $t2 $t3\n";
  }
  if (m/^([0-9a-f]{32})$/ || m/^([0-9a-f]{32})  -/ || m/^([0-9a-f]{32})  \.\/temp/ || m/^MD5 .+ = ([0-9a-f]{32})$/) {
    $md5sum=$1;
#   print "md5sum=$md5sum\n";
    $results{$file}{"error"}=$error;
    $results{$file}{"md5"}=$md5sum;
    $results{$file}{"time1"}=$t1;
    $results{$file}{"time2"}=$t2;
    $results{$file}{"time3"}=$t3;
    $results{$file}{"time4"}=0;
  }
  }
}

close(F);

if ($input2) {
  open (F,"<$input2") || die "file $input2 not found";
  while(<F>) {
    if (m|Segmentation fault .+ ./temp/(\S+).log 2|) {
      my $file=$1;
      my $pdfwrite=0;
      $pdfwrite=1 if (m/pdfwrite/);
      $pdfwrite=1 if (m/ps2write/);
#     print "$pdfwrite $file\n";
      if (exists $results{$file}{"error"}) {
        $results{$file}{"error"}=7 if ($pdfwrite==1);
        $results{$file}{"error"}=8 if ($pdfwrite==0 && ($results{$file}{"error"} % 2 == 0 || $results{$file}{"error"} == -1));
      } else {
#       die "$file not found in ressults";
      }
    }
  }
  close(F);
}

delete $results{"gs_build"} if (exists $results{"gs_build"});

open(F,">$output") || die "file $output can't be written to";
foreach (sort keys %results) {
  $results{$_}{"md5"}=0 if ($results{$_}{"error"}!=0);
  print F "$_\t$results{$_}{'error'}\t$results{$_}{'time1'}\t$results{$_}{'time2'}\t$results{$_}{'time3'}\t$results{$_}{'time4'}\t$results{$_}{'md5'}\t$rev\t$results{$_}{'product'}\t$machine\n";
}
close(F);

if (0) {
  foreach (sort keys %results) {
    print "$_\n" if ($results{$_}{'error'} != 0);
  }
}

