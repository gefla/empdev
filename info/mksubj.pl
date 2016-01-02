#!/usr/bin/perl
#
#   Empire - A multi-player, client/server Internet based war game.
#   Copyright (C) 1986-2016, Dave Pare, Jeff Bailey, Thomas Ruschak,
#                 Ken Stevens, Steve McClure, Markus Armbruster
#
#   Empire is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#   ---
#
#   See files README, COPYING and CREDITS in the root of the source
#   tree for related information and legal notices.  It is expected
#   that future projects/authors will amend these files as needed.
#
#   ---
#
#   mksubj.pl: Update the subject index pages
#
#   Known contributors to this file:
#      Ken Stevens (when it was still info.pl)
#      Markus Armbruster, 2006-2014
#
# Usage: mksubj.pl SUBJECT... INFO-FILE...
#
# Read the INFO-FILE..., update info/SUBJECT.t for each SUBJECT.

use strict;
use warnings;

# The chapters, in order
my @Chapters = qw/Introduction Concept Command Server/;

my @Levels = qw/Basic Expert Obsolete/;

# $Subjects{SUBJECT} is a reference to an an anonymous array
# containing SUBJECT's topics
my %Subjects;

# $filename{TOPIC} is TOPIC's file name
my %filename;
# $lines{TOPIC} is the number of lines in $filename{TOPIC}
my %lines;
# $chapter{TOPIC} is TOPIC's chapter (first arg to .TH)
my %chapter;
# $desc{TOPIC} is a one line description of TOPIC (second arg to .NA)
my %desc;
# $level{TOPIC} is TOPIC's difficulty level (arg to .LV)
my %level;

# current info file
my $filename;

while ($#ARGV >= 0 && $ARGV[0] !~ /\.t$/) {
    $Subjects{shift @ARGV} = [];
}

for (@ARGV) {
    $filename{fn2topic($_)} = $_;
}

for (@ARGV) {
    parse_file($_);
}

for (keys %Subjects) {
    update_subj($_);
}

write_toc();

sub fn2topic {
    my ($fn) = @_;
    $fn =~ s,.*/([^/]*)\.t$,$1,;
    return $fn;
}

# Parse an info file
# Set $filename, $lines{TOPIC}, $chapter{TOPIC}, $desc{TOPIC},
# $level{TOPIC}.
# Update %Subjects.
sub parse_file {
    ($filename) = @_;
    my $topic = fn2topic($filename);

    open(F, "<$filename")
	or die "Can't open $filename: $!";

    $_ = <F>;
    if (/^\.TH (\S+) (\S.+\S)$/) {
	if (!grep(/^$1$/, @Chapters)) {
	    error("First argument to .TH was '$1', which is not a known chapter");
	}
	$chapter{$topic} = $1;
	if ($1 eq "Command" && $2 ne "\U$topic") {
	    error("Second argument to .TH was '$2' but it should be '\U$topic'");
	}
    } else {
	error("The first line in the file must be a .TH request");
    }

    $_ = <F>;
    if (/^\.NA (\S+) "(\S.+\S)"$/) {
	if ($topic ne $1) {
	    error("First argument to .NA was '$1' but it should be '$topic'");
	}
	$desc{$topic} = $2;
    } else {
	error("The second line in the file must be a .NA request");
    }

    $_ = <F>;
    if (/^\.LV (\S+)$/) {
	if (!grep(/^$1$/, @Levels)) {
	    error("The argument to .LV was '$1', which is not a known level");
	}
	$level{$topic} = $1;
    } else {
	error("The third line in the file must be a .LV request");
    }

    while (<F>) {
	last if /^\.SA/;
    }

    if ($_) {
	if (/^\.SA "([^\"]*)"/) {
	    parse_see_also($topic, $1);
	} else {
	    error("Incorrect .SA argument, expecting '.SA \"item1, item2\"'");
	}
    } else {
	error(".SA request is missing");
    }

    if (<F>) {
	error(".SA request must be the last line");
    }

    $lines{$topic} = $.;
    close F;
}

sub parse_see_also {
    my ($topic, $sa) = @_;
    my $wanted = $chapter{$topic};
    my $found;		       # found a subject?

    $wanted = undef if $wanted eq 'Concept' or $wanted eq 'Command';

    for (split(/, /, $sa)) {
	next if exists $filename{$_};
	error("Unknown topic $_ in .SA") unless exists $Subjects{$_};
	push @{$Subjects{$_}}, $topic;
	$found = 1;
	if ($wanted && $_ eq $wanted) {
	    $wanted = undef;
	}
    }

    error("No subject listed in .SA") unless $found;
    error("Chapter $wanted not listed in .SA") if $wanted;
}

# Update a Subject.t file
sub update_subj {
    my ($subj) = @_;
    my $fname = "info/$subj.t";
    my @topics = @{$Subjects{$subj}};
    my $out = "";
    my ($any_topic, $any_basic, $any_obsolete, $any_long);

    my $largest = "";
    for my $topic (@topics) {
	$largest = $topic if length $topic > length $largest;
    }

    $out .= '.\" DO NOT EDIT THIS FILE.  It was automatically generated by mksubj.pl'."\n";
    $out .= ".TH Subject \U$subj\n";
    $largest =~ s/-/M/g;
    $out .= ".in \\w'$largest" . "XX\\0\\0\\0\\0'u\n";

    for my $chap (@Chapters) {
	my $empty = 1;
	for my $topic (@topics) {
	    $any_topic = 1;
	    next if $chapter{$topic} ne $chap;
	    $out .= ".s1\n" if $empty;
	    $empty = 0;
	    my $flags = "";
	    if ($level{$topic} eq 'Basic') {
		$flags .= "*";
		$any_basic = 1;
	    }
	    if ($level{$topic} eq 'Obsolete') {
		$flags .= "+";
		$any_obsolete = 1;
	    }
	    if ($lines{$topic} > 300) {
		# TODO use formatted line count
		$flags .= "!";
		$any_long = 1;
	    }
	    $flags = sprintf("%-2s", $flags);
	    $out .= ".L \"$topic $flags\"\n";
	    $out .= "$desc{$topic}\n";
	}
    }
    unless ($any_topic) {
	print STDERR "$0: Subject $subj has no topics\n";
	exit 1;
    }
    $out .= ".s1\n"
	. ".in 0\n"
	. "For info on a particular topic, type \"info <topic>\" where <topic> is\n"
	. "one of the topics listed above.\n";
    $out .= "Topics marked by * are the most important and should be read by new players.\n"
	if $any_basic;
    $out .= "Topics marked by + are obsolete.\n"
	if $any_obsolete;
    $out .= "Topics with unusually long info are marked with a !.\n"
	if $any_long;

    return if (same_contents($fname, $out));
    open(SUBJ, ">$fname")
	or die "Can't open $fname for writing: $!";
    print SUBJ $out;
    close SUBJ;
}

sub same_contents {
    my ($fname, $contents) = @_;
    local $/;

    if (!open(SUBJ, "<$fname")) {
	return 0 if ($!{ENOENT});
	die "Can't open $fname for reading: $!";
    }
    my $old = <SUBJ>;
    close SUBJ;
    return $contents eq $old;
}

sub write_toc {
    my @toc;
    for (keys %chapter) {
	push @toc, "$chapter{$_} $_";
    }
    open(TOC, ">info/toc")
	or die "Can't open info/toc for writing: $!";
    print TOC join("\n", sort @toc);
    close TOC;
}

# Print an integrity error message and exit with code 1
sub error {
    my ($error) = @_;

    print STDERR "mksubj.pl:$filename:$.: $error\n";
    exit 1;
}
