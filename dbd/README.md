# NAME

dbd - Database helper tool.

# SYNOPSIS

```
dbd [-v] [-h] [-q] [-t] [-s] [-e] [-o file] [-d driver] [-p params] table|query|escape
```

# DESCRIPTION

The tool allows queries to be made to a sql database, with the formatting
of the data controlled by the caller. This tool is designed to make database
scripting easier, avoiding the need for text manipulation.

If a table name is specified, a query will be automatically created to select
all data in that table. Alternatively, the query can be specified exactly using
the query option.

By default, output is 'echo' encoded, columns are tab separated, and lines are
ended with a line feed. To get unformatted data, use the 'none' encoding, and
suppress the trailing linefeed with --no-end-of-line.

# OPTIONS

    -o, --file-out file		File to write to. Defaults to stdout.

    -d, --driver driver		Name of the driver to use for database access. If unspecified, read from DBD_DRIVER.

    -p, --params params		Parameter string to pass to the database. If unspecified, read from DBD_PARAMS.

    -q, --query			Query string to run against the database. Expected to return number of rows affected.

    -e, --escape			Escape the arguments against the given database, using appropriate escaping for that database.

    -s, --select			Run select queries against the database. Expected to return database rows as results.

    -t, --table			Run select queries against the tables in the given database. Expected to return database rows as results.

    -a, --argument val		Pass an argument to a prepared statement.

    -f, --file-argument file	Pass a file containing argument to a prepared statement. '-' for stdin.

    -z, --null-argument		Pass a NULL value as an argument to a prepared statement.

    -c, --end-of-column end	Use separator between columns.

    -l, --end-of-line end		Use separator between lines.

    --header			Output a header on the first line.

    -n, --no-end-of-line		No separator on last line.

    -x, --encoding encoding	Encoding to use. One of 'none', 'base64', 'base64url', 'echo'.

    -h, --help			Display this help message.

    -v, --version			Display the version number.

# RETURN VALUE

The dbd tool returns a non zero exit code if the tool is unable to successfully
run the query, or if output cannot be written to stdout.

# EXAMPLES

In this example, we query all contents of the given table.

```
~$ dbd -d "sqlite3" -p "/tmp/database.sqlite3" -t "users" 
```

In this example, we submit a query with arguments.

```
~$ dbd -d "sqlite3" -p "/tmp/database.sqlite3" -a "1" -s "select * from users where id = %s" 
```

In this example, we want the raw value of a single row and column.

```
~$ dbd -d "sqlite3" -p "/tmp/database.sqlite3" -x "none" -n -a "1" \
  -s "select certificate from users where id = %s" 
-----BEGIN CERTIFICATE-----
...
-----END CERTIFICATE-----
```

Here we escape a dangerous string.

```
~$ dbd -d "sqlite3" -p "/tmp/database.sqlite3" -e "john';drop table users" 
john'';drop table users
```

