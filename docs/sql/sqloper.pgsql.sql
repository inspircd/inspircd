CREATE TABLE ircd_opers (
    "id" serial NOT NULL,
    "name" text NOT NULL,
    "password" text NOT NULL,
    "hash" text,
    "host" text NOT NULL,
    "type" text NOT NULL,
    "fingerprint" text,
    "autologin" smallint NOT NULL DEFAULT 0,
    "active" smallint NOT NULL DEFAULT 1
);
ALTER TABLE ONLY ircd_opers
    ADD CONSTRAINT ircd_opers_pkey PRIMARY KEY (id);
