CREATE TABLE IF NOT EXISTS `client621_account` (
	`user_id` text PRIMARY KEY NOT NULL,
	`account_id` text NOT NULL
);
--> statement-breakpoint
CREATE UNIQUE INDEX IF NOT EXISTS `client621_account_account_id_unique` ON `client621_account` (`account_id`);--> statement-breakpoint
CREATE TABLE IF NOT EXISTS `client621_asset` (
	`template_id` text PRIMARY KEY NOT NULL,
	`build` text NOT NULL,
	`proof_sha256` text NOT NULL,
	`path` text NOT NULL,
	`verified` integer DEFAULT 0 NOT NULL
);
--> statement-breakpoint
CREATE TABLE IF NOT EXISTS `client621_exchange` (
	`code_hash` text PRIMARY KEY NOT NULL,
	`user_id` text NOT NULL,
	`parent_session` text NOT NULL,
	`expires_at` integer NOT NULL,
	`used_at` integer
);
--> statement-breakpoint
CREATE TABLE IF NOT EXISTS `client621_profile` (
	`user_id` text PRIMARY KEY NOT NULL,
	`revision` integer NOT NULL,
	`equipment` text NOT NULL,
	`inventory_fingerprint` text NOT NULL,
	`updated_at` integer NOT NULL
);
--> statement-breakpoint
CREATE TABLE IF NOT EXISTS `client621_session` (
	`token_hash` text PRIMARY KEY NOT NULL,
	`user_id` text NOT NULL,
	`parent_session` text NOT NULL,
	`expires_at` integer NOT NULL
);
