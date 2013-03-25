#!perl
# Generates info for Module::CoreList from this perl tree
# run this from the root of a perl tree
#
# Data is on STDOUT.
#
# With an optional arg specifying the root of a CPAN mirror, outputs the
# %upstream and %bug_tracker hashes too.

use autodie;
use strict;
use warnings;
use File::Find;
use ExtUtils::MM_Unix;
use version;
use lib "Porting";
use Maintainers qw(%Modules files_to_modules);
use File::Spec;
use Parse::CPAN::Meta;
use IPC::Cmd 'can_run';
use HTTP::Tiny;
use IO::Uncompress::Gunzip;

my $corelist_file = 'dist/Module-CoreList/lib/Module/CoreList.pm';
my $pod_file = 'dist/Module-CoreList/lib/Module/CoreList.pod';

my %lines;
my %module_to_file;
my %modlist;

die "usage: $0 [ cpan-mirror/ ] [ 5.x.y] \n" unless @ARGV <= 2;
my $cpan         = shift;
my $raw_version  = shift || $];
my $perl_version = version->parse("$raw_version");
my $perl_vnum    = $perl_version->numify;
my $perl_vstring = $perl_version->normal; # how do we get version.pm to not give us leading v?
$perl_vstring =~ s/^v//;

if ( !-f 'MANIFEST' ) {
    die "Must be run from the root of a clean perl tree\n";
}

open( my $corelist_fh, '<', $corelist_file );
my $corelist = join( '', <$corelist_fh> );

if ($cpan) {
    my $modlistfile = File::Spec->catfile( $cpan, 'modules', '02packages.details.txt' );
    my $content;

    my $fh;
    if ( -e $modlistfile ) {
        warn "Reading the module list from $modlistfile";
        open $fh, '<', $modlistfile;
    } elsif ( -e $modlistfile . ".gz" ) {
        my $zcat = can_run('gzcat') || can_run('zcat') or die "Can't find gzcat or zcat";
        warn "Reading the module list from $modlistfile.gz";
        open $fh, '-|', "$zcat $modlistfile.gz";
    } else {
        warn "About to fetch 02packages from ftp.funet.fi. This may take a few minutes\n";
	my $gzipped_content = fetch_url('http://ftp.funet.fi/pub/CPAN/modules/02packages.details.txt.gz');
	unless ($gzipped_content) {
            die "Unable to read 02packages.details.txt from either your CPAN mirror or ftp.funet.fi";
        }
	IO::Uncompress::Gunzip::gunzip(\$gzipped_content, \$content, Transparent => 0)
	    or die "Can't gunzip content: $IO::Uncompress::Gunzip::GunzipError";
    }

    if ( $fh and !$content ) {
        local $/ = "\n";
        $content = join( '', <$fh> );
    }

    die "Incompatible modlist format"
        unless $content =~ /^Columns: +package name, version, path/m;

    # Converting the file to a hash is about 5 times faster than a regexp flat
    # lookup.
    for ( split( qr/\n/, $content ) ) {
        next unless /^([A-Za-z_:0-9]+) +[-0-9.undefHASHVERSIONvsetwhenloadingbogus]+ +(\S+)/;
        $modlist{$1} = $2;
    }
}

find(
    sub {
        /(\.pm|_pm\.PL)$/ or return;
        /PPPort\.pm$/ and return;
        my $module = $File::Find::name;
        $module =~ /\b(demo|t|private)\b/ and return;    # demo or test modules
        my $version = MM->parse_version($_);
        defined $version or $version = 'undef';
        $version =~ /\d/ and $version = "'$version'";

        # some heuristics to figure out the module name from the file name
        $module =~ s{^(lib|cpan|dist|(?:symbian/)?ext)/}{}
			and $1 ne 'lib'
            and (
            $module =~ s{\b(\w+)/\1\b}{$1},
            $module =~ s{^B/O}{O},
            $module =~ s{^Devel-PPPort}{Devel},
            $module =~ s{^libnet/}{},
            $module =~ s{^Encode/encoding}{encoding},
            $module =~ s{^IPC-SysV/}{IPC/},
            $module =~ s{^MIME-Base64/QuotedPrint}{MIME/QuotedPrint},
            $module =~ s{^(?:DynaLoader|Errno|Opcode|XSLoader)/}{},
            $module =~ s{^Sys-Syslog/win32}{Sys-Syslog},
            $module =~ s{^Time-Piece/Seconds}{Time/Seconds},
            );
        $module =~ s{^vms/ext}{VMS};
		$module =~ s{^lib/}{}g;
        $module =~ s{/}{::}g;
        $module =~ s{-}{::}g;
		$module =~ s{^.*::lib::}{}; # turns Foo/lib/Foo.pm into Foo.pm
        $module =~ s/(\.pm|_pm\.PL)$//;
        $lines{$module}          = $version;
        $module_to_file{$module} = $File::Find::name;
    },
    'vms/ext',
    'symbian/ext',
    'lib',
    'ext',
	'cpan',
	'dist'
);

-e 'configpm' and $lines{Config} = 'undef';

if ( open my $ucdv, "<", "lib/unicore/version" ) {
    chomp( my $ucd = <$ucdv> );
    $lines{Unicode} = "'$ucd'";
    close $ucdv;
}

my $versions_in_release = "    " . $perl_vnum . " => {\n";
foreach my $key ( sort keys %lines ) {
    $versions_in_release .= sprintf "\t%-24s=> %s,\n", "'$key'", $lines{$key};
}
$versions_in_release .= "    },\n";

$corelist =~ s/^(%version\s*=\s*.*?)(^\);)$/$1$versions_in_release$2/xism;

exit unless %modlist;

# We have to go through this two stage lookup, given how Maintainers.pl keys its
# data by "Module", which is really a dist.
my $file_to_M = files_to_modules( values %module_to_file );

my %module_to_upstream;
my %module_to_dist;
my %dist_to_meta_YAML;
my %module_to_deprecated;
while ( my ( $module, $file ) = each %module_to_file ) {
    my $M = $file_to_M->{$file};
    next unless $M;
    next if $Modules{$M}{MAINTAINER} && $Modules{$M}{MAINTAINER} eq 'p5p';
    $module_to_upstream{$module} = $Modules{$M}{UPSTREAM};
    $module_to_deprecated{$module} = 1 if $Modules{$M}{DEPRECATED};
    next
        if defined $module_to_upstream{$module}
            && $module_to_upstream{$module} =~ /^(?:blead|first-come)$/;
    my $dist = $modlist{$module};
    unless ($dist) {
        warn "Can't find a distribution for $module\n";
        next;
    }
    $module_to_dist{$module} = $dist;

    next if exists $dist_to_meta_YAML{$dist};

    $dist_to_meta_YAML{$dist} = undef;

    # Like it or lump it, this has to be Unix format.
    my $meta_YAML_path = "authors/id/$dist";
    $meta_YAML_path =~ s/(?:tar\.gz|tar\.bz2|zip|tgz)$/meta/ or die "$meta_YAML_path";
    my $meta_YAML_url = 'http://ftp.funet.fi/pub/CPAN/' . $meta_YAML_path;

    if ( -e "$cpan/$meta_YAML_path" ) {
        $dist_to_meta_YAML{$dist} = Parse::CPAN::Meta::LoadFile( $cpan . "/" . $meta_YAML_path );
    } elsif ( my $content = fetch_url($meta_YAML_url) ) {
        unless ($content) {
            warn "Failed to fetch $meta_YAML_url\n";
            next;
        }
        eval { $dist_to_meta_YAML{$dist} = Parse::CPAN::Meta::Load($content); };
        if ( my $err = $@ ) {
            warn "$meta_YAML_path: ".$err;
            next;
        }
    } else {
        warn "$meta_YAML_path does not exist for $module\n";

        # I tried code to open the tarballs with Archive::Tar to find and
        # extract META.yml, but only Text-Tabs+Wrap-2006.1117.tar.gz had one,
        # so it's not worth including.
        next;
    }
}

my $upstream_stanza = "%upstream = (\n";
foreach my $module ( sort keys %module_to_upstream ) {
    my $upstream = defined $module_to_upstream{$module} ? "'$module_to_upstream{$module}'" : 'undef';
    $upstream_stanza .= sprintf "    %-24s=> %s,\n", "'$module'", $upstream;
}
$upstream_stanza .= ");";

$corelist =~ s/^%upstream .*? ;$/$upstream_stanza/ismx;

# Deprecation generation
my $deprecated_stanza = "    " . $perl_vnum . " => {\n";
foreach my $module ( sort keys %module_to_deprecated ) {
    my $deprecated = defined $module_to_deprecated{$module} ? "'$module_to_deprecated{$module}'" : 'undef';
    $deprecated_stanza .= sprintf "\t%-24s=> %s,\n", "'$module'", $deprecated;
}
$deprecated_stanza .= "    },\n";
$corelist =~ s/^(%deprecated\s*=\s*.*?)(^\);)$/$1$deprecated_stanza$2/xism;

my $tracker = "%bug_tracker = (\n";
foreach my $module ( sort keys %module_to_upstream ) {
    my $upstream = defined $module_to_upstream{$module};
    next
        if defined $upstream
            and $upstream eq 'blead' || $upstream eq 'first-come';

    my $bug_tracker;

    my $dist = $module_to_dist{$module};
    $bug_tracker = $dist_to_meta_YAML{$dist}->{resources}{bugtracker}
        if $dist;

    $bug_tracker = defined $bug_tracker ? "'$bug_tracker'" : 'undef';
	next if $bug_tracker eq "'http://rt.perl.org/perlbug/'";
    $tracker .= sprintf "    %-24s=> %s,\n", "'$module'", $bug_tracker;
}
$tracker .= ");";

$corelist =~ s/^%bug_tracker .*? ;/$tracker/eismx;

unless (
    $corelist =~ /^%released \s* = \s* \(
        .*?
        $perl_vnum => .*?
        \);/ismx
    )
{
    warn "Adding $perl_vnum to the list of released perl versions. Please consider adding a release date.\n";
    $corelist =~ s/^(%released \s* = \s* .*?) ( \) )
                /$1  $perl_vnum => '????-??-??',\n  $2/ismx;
}

write_corelist($corelist,$corelist_file);

open( my $pod_fh, '<', $pod_file );
my $pod = join( '', <$pod_fh> );

unless ( $pod =~ /and $perl_vstring releases of perl/ ) {
    warn "Adding $perl_vstring to the list of perl versions covered by Module::CoreList\n";
    $pod =~ s/(currently covers (?:.*?))\s*and (.*?) releases of perl/$1, $2 and $perl_vstring releases of perl/ism;
}

write_corelist($pod,$pod_file);

warn "All done. Please check over $corelist_file and $pod_file carefully before committing. Thanks!\n";


sub write_corelist {
    my $content = shift;
    my $filename = shift;
    open (my $clfh, ">", $filename);
    print $clfh $content;
    close($clfh);
}

sub fetch_url {
    my $url = shift;
    my $http = HTTP::Tiny->new;
    my $response = $http->get($url);
    if ($response->{success}) {
	return $response->{content};
    } else {
	warn "Error fetching $url: $response->{status} $response->{reason}\n";
        return;
    }
}
