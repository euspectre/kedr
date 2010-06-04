#!/usr/bin/perl -w

# Verify number of arguments (should be 1 - function name).
$#ARGV == 0 or die "Usage is:\n\n\tlook_kernel_function function_name\n\n";
my $function_name = shift;
# Simple verification, that function name is valid name.
$function_name =~ /^\w+$/ or die "Function name should contain only letters, digits and underscore('_')";
# Open /proc/kallsyms (hardcoded) for get list of symbols.
open(FILE, "/proc/kallsyms") or die "Could not open /proc/kallsyms";

my $result = "false";
while(my $line = <FILE>)
{
    if($line =~ /^[\da-f]+\s+T\s+$function_name$/)
    {
        $result = "true";
        last;
    }
}
close FILE;

if($result =~ /true/)
{
    print("function $function_name is exported by the kernel\n");
    exit 0;
}
else
{
    print("function $function_name isn't exported by the kernel\n");
    exit 1;
}
