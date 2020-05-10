/^```bash/ { prefix=1; print $0; next; }
/^```$/ { prefix=0; print $0; next; }
{
    if ( prefix==1 )
        print "$ " $0
    else
        print $0
}
# Don't prefix here-documents. Assume it's the rest of the cell for now.
/<</ { prefix=0 }
# Same for function definitions
/{/ { prefix=0 }
