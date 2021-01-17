# NAME

endec - Encode / decode / escape / unescape data.

# SYNOPSIS

```
endec [-v] [-h] [-r file] [-w file] [...] [string]
```

# DESCRIPTION

The  tool  applies  each  specified transformation to the given data in
turn, returning the result on stdout.

In most typical scenarios, data in one format needs to  be  decoded  or
unescaped  from a source format and then immediately encoded or escaped
into another format for safe use. By  specifying  multiple  transformations
data can be be passed from one encoding to another.

Decoding  and  unescaping is performed securely, meaning that any input
data that cannot be decoded or unescaped will cause the  tool  to  exit
with a non zero exit code.

# OPTIONS

       -u, --url-escape
              URL escape data as defined in HTML5

       -U, --url-unescape
              URL unescape data as defined in HTML5

       -f, --form-escape
              URL  escape  data  as defined in HTML5, with spaces converted to
              '+'

       -F, --form-unescape
              URL unescape data as defined in HTML5,  with  '+'  converted  to
              spaces

       -p, --path-escape
              Escape a filesystem path to be embedded in a URL

       -e, --entity-escape
              Entity escape data for XML

       -E, --entity-unescape
              Entity unescape data for XML

       -c, --echo-escape
              Shell escape data as per echo

       --echoquote-escape
              Shell escape data as per echo, including quotes

       -l, --ldap-escape
              LDAP escape data as per RFC4514 and RFC4515

       --ldapdn-escape
              LDAP escape distinguished name data as per RFC4514

       --ldapfilter-escape
              LDAP escape filter data as per RFC4515

       -b, --base64-encode
              Encode data as base64 as per RFC4648 section 4

       --base64url-encode
              Encode data as base64url as per RFC4648 section 5

       --base64url-nopad-encode
              Encode data as base64url with no padding as per rfc7515 appendix
              C

       -B, --base64-decode
              Decode data as base64 or base64url

       -t, --base32-encode
              Encode data as base32 as per RFC4648 section 6

       --base32hex-encode
              Encode data as base32hex as per RFC4648 section 7

       --base32hex-nopad-encode
              Encode data as base32hex with no padding as per RFC4648  section
              7

       -T, --base32-decode
              Decode data as base32

       --base32hex-decode
              Decode data as base32hex

       -s, --base16-encode
              Encode data as base16 as per RFC4648 section 8

       --base16colon-encode
              Encode data as base16 separated with colons

       --base16-lower-encode
              Encode data as base16 in lower case

       --base16colon-lower-encode
              Encode data as base16 with colons in lower case

       -S, --base16-decode
              Decode data as base16

       -r, --read
              File to read from. Defaults to stdin.

       -w, --write
              File to write to. Defaults to stdout.

       -h, --help
              Display this help message.

       -v, --version
              Display the version number.

# RETURN VALUE

The endec tool returns a non zero exit code if invalid data was encountered during decoding.

# EXAMPLES

In this example, we decode the base64 string, then  entity  encode  the
result.

```
~$ endec --base64-decode --entity-escape "VGhpcyAmIHRoYXQK"
This &amp; that
```
