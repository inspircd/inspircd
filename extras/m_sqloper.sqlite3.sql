CREATE TABLE ircd_opers (
id integer primary key,
username text,
password text,
hostname text,
type text,
active integer NOT NULL DEFAULT 1);
