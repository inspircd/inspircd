CREATE TABLE [dbo].[ircd_opers] (
  [id] int IDENTITY(1, 1) NOT NULL,
  [username] varchar(255) NULL,
  [password] varchar(255) NULL,
  [hostname] varchar(255) NULL,
  [type] varchar(255) NULL,
  [active] bit NOT NULL DEFAULT 1,
  PRIMARY KEY CLUSTERED ([id])
)
