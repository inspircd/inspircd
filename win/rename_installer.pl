#!perl

open(FH,"<../src/version.sh") or die("Can't open version.sh");
while (chomp($v = <FH>))
{
	$version = $v if $v =~ /^echo/;
}
close FH;

print "Version: '$version'\n";

$version =~ /InspIRCd-(\d+)\.(\d+)\.(\d+)([ab\+]|RC|rc)/;

$v1 = $1;
$v2 = $2;
$v3 = $3;
$type = $4;

print "v1=$1 v2=$2 v3=$3 type=$4\n";

if ($type =~ /^[ab]|rc|RC$/)
{
	$version =~ /InspIRCd-\d+\.\d+\.\d+([ab]|RC|rc)(\d+)/;
	$alphabeta = $2;
	print "Version sub is $type $alphabeta\n";
	$name = "InspIRCd-$v1.$v2.$v3$type$alphabeta.exe";
	$rel = "$v1.$v2.$v3$type$alphabeta";
}
else
{
	$name = "InspIRCd-$v1.$v2.$v3.exe";
	$rel = "$v1.$v2.$v3";
}

print "del $name\n";
print "ren Setup.exe $name\n";

system("del $name");
system("ren Setup.exe $name");

system("upload_release.bat $name $rel");
