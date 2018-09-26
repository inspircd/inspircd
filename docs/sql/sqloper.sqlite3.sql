CREATE TABLE ircd_opers (
id integer primary key,
name text NOT NULL,
password text NOT NULL,
hash text,
host text NOT NULL,
type text NOT NULL,
fingerprint text,
autologin integer NOT NULL DEFAULT 0,
active integer NOT NULL DEFAULT 1);
