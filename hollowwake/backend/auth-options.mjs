import { bearer, deviceAuthorization, username } from 'better-auth/plugins';
export const CLIENT_ID = 'hollowwake-uwp';
export function authOptions(config, database, send) {
  return {
    appName: 'Hollowwake', baseURL: config.origin, basePath: '/api/auth', secret: config.secret, database,
    trustedOrigins: [config.origin],
    logger: { disabled: true },
    emailAndPassword: { enabled: true, minPasswordLength: 12, maxPasswordLength: 128,
      requireEmailVerification: config.requireEmailVerification, revokeSessionsOnPasswordReset: true,
      sendResetPassword: async ({ user, token }) => {
        const recovery = new URL('/reset', config.origin); recovery.hash = new URLSearchParams({ token }).toString();
        await send({ to: user.email, subject: 'Reset your Hollowwake password', text: `You requested an account recovery link. Open it privately: ${recovery.href}\nIgnore this message if you did not request it.` });
      } },
    emailVerification: { sendOnSignUp: config.requireEmailVerification, autoSignInAfterVerification: false,
      sendVerificationEmail: async ({ user, url }) => send({ to: user.email, subject: 'Verify your Hollowwake email', text: `Verify your email by opening this link privately: ${url}` }) },
    session: { expiresIn: config.sessionSeconds, updateAge: config.sessionUpdateSeconds,
      cookieCache: { enabled: false } },
    advanced: { useSecureCookies: !config.local, cookiePrefix: 'hollowwake',
      defaultCookieAttributes: { httpOnly: true, sameSite: 'lax', secure: !config.local },
      database: { generateId: 'uuid' } },
    // /device is authenticated code review; keep global traffic protection, not a typo lockout.
    rateLimit: { enabled: true, storage: 'database', window: 60, max: 120, customRules: { '/device': false } },
    plugins: [username({ minUsernameLength: 3, maxUsernameLength: 24, usernameValidator: value => /^[a-zA-Z0-9_]+$/.test(value) }), bearer(),
      deviceAuthorization({ verificationUri: `${config.origin}/link`, expiresIn: config.deviceExpires,
        interval: config.pollInterval, deviceCodeLength: 64, userCodeLength: 8,
        validateClient: clientId => clientId === CLIENT_ID })]
  };
}
