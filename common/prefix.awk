/^```bash/ { prefix=1; print $0; next; }
/^```$/ { prefix=0; print $0; next; }
{
    if ( prefix==1 )
        print "$ " $0
    else
        print $0
}
