use strict;
use warnings;
use Test::More tests => 6;

use Set::Object qw(set); # RT #93516 by smls75@gmail.com

is(set( set() )->size, 1);
is(set( set(), "foo" )->size, 2);
is(set( set(), set() )->size, 2);
is(set( "foo", set() )->size, 2);

is(set( set(2, 4, 6), "foo" )->size, 2);
is(set( "foo", set(2, 4, 6) )->size, 2);
