CREATE TABLE ircd_log (
id integer primary key,
category_id integer,
nick integer,
host integer,
source integer,
dtime integer);


CREATE TABLE ircd_log_categories (
  category_id integer primary key,
  category text NOT NULL
);
INSERT INTO "ircd_log_categories" VALUES(1, 'Oper');
INSERT INTO "ircd_log_categories" VALUES(2, 'Kill');
INSERT INTO "ircd_log_categories" VALUES(3, 'Server Link');
INSERT INTO "ircd_log_categories" VALUES(4, 'G/Z/K/E Line');
INSERT INTO "ircd_log_categories" VALUES(5, 'Connect');
INSERT INTO "ircd_log_categories" VALUES(6, 'Disconnect');
INSERT INTO "ircd_log_categories" VALUES(7, 'Flooding');
INSERT INTO "ircd_log_categories" VALUES(8, 'Load Module');


CREATE TABLE ircd_log_actors (
  id integer primary key,
  actor text
);


CREATE TABLE ircd_log_hosts (
  id integer primary key,
  hostname text
);

