#pragma once
// Actual Microsoft LDAP provider, scoped to the explicitly configured backend.
// This is optional LDAP library compatibility, not Fortnite's HTTP login flow.
void* ResolveLdapImport(const char* dll,const char* symbol);
