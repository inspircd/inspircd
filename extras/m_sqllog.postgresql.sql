--
-- PostgreSQL database dump
--

CREATE TABLE ircd_log (
    id serial NOT NULL,
    category_id bigint,
    nick bigint,
    host bigint,
    source bigint,
    dtime bigint DEFAULT 0 NOT NULL
);
ALTER TABLE ONLY ircd_log
    ADD CONSTRAINT ircd_log_pkey PRIMARY KEY (id);



CREATE TABLE ircd_log_actors (
    id serial NOT NULL,
    actor text
);
ALTER TABLE ONLY ircd_log_actors
    ADD CONSTRAINT ircd_log_actors_pkey PRIMARY KEY (id);



CREATE TABLE ircd_log_categories (
    category_id serial NOT NULL,
    category text NOT NULL
);

INSERT INTO ircd_log_categories VALUES (1, 'Oper');
INSERT INTO ircd_log_categories VALUES (2, 'Kill');
INSERT INTO ircd_log_categories VALUES (3, 'Server Link');
INSERT INTO ircd_log_categories VALUES (4, 'G/Z/K/E Line');
INSERT INTO ircd_log_categories VALUES (5, 'Connect');
INSERT INTO ircd_log_categories VALUES (6, 'Disconnect');
INSERT INTO ircd_log_categories VALUES (7, 'Flooding');
INSERT INTO ircd_log_categories VALUES (8, 'Load Module');

ALTER TABLE ONLY ircd_log_categories
    ADD CONSTRAINT ircd_log_categories_pkey PRIMARY KEY (category_id);



CREATE TABLE ircd_log_hosts (
    id serial NOT NULL,
    hostname text
);
ALTER TABLE ONLY ircd_log_hosts
    ADD CONSTRAINT ircd_log_hosts_pkey PRIMARY KEY (id);
