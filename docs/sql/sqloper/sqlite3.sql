CREATE TABLE IF NOT EXISTS "ircd_opers" (
  "active" bool NOT NULL DEFAULT true,

  "name" text NOT NULL,
  "password" text NOT NULL,
  "host" text NOT NULL,
  "type" text NOT NULL,

  "autologin" CHECK ("autologin" IN ('strict', 'relaxed', 'never')),
  "class" text,
  "hash" text,
  "maxchans" bigint,
  "nopassword" bool,
  "vhost" text,

  "commands" text,
  "privs" text,
  "chanmodes" text,
  "usermodes" text,
  "snomasks" text,

  "account" text,
  "autojoin" text,
  "automotd" bool,
  "fingerprint" text,
  "level" bigint,
  "modes" text,
  "motd" bool,
  "override" text,
  "sslonly" bool,
  "swhois" text
);
