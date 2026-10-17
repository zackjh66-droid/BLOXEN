import { betterAuth } from 'better-auth';
import nodemailer from 'nodemailer';
import { authOptions } from './auth-options.mjs';
export { CLIENT_ID } from './auth-options.mjs';
export function createAuth(config, database, sendMailOverride) {
  const mailer = config.smtp ? nodemailer.createTransport(config.smtp) : null;
  const send = sendMailOverride || (async message => {
    if (!mailer) throw new Error('Mail delivery is not configured');
    await mailer.sendMail({ from: config.smtpFrom, ...message });
  });
  return betterAuth(authOptions(config, database, send));
}
