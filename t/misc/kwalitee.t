# -*- perl -*-
use strict;
use warnings;

use Test::More;
use Config;

plan skip_all => 'This test is only run for the module author'
    unless -d '.git' || $ENV{RELEASE_TESTING};
plan skip_all => 'Test::Kwalitee fails with clang -faddress-sanitizer'
    if $Config{ccflags} =~ /-faddress-sanitizer/;

if (-e 'MYMETA.yml' and !-e 'META.yml') {
  require File::Copy;
  File::Copy::cp('MYMETA.yml','META.yml') ;
}

eval {
  require Test::Kwalitee;
  my @args;
  if ($Test::Kwalitee::VERSION lt '1.02') {
    @args = (tests => [ qw( -proper_libs ) ]);
  }
  Test::Kwalitee->import(@args);
};
plan skip_all => "Test::Kwalitee needed for testing kwalitee"
    if $@;
