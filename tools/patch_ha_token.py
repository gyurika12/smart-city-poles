# -*- coding: utf-8 -*-
"""Removes the hard-coded HA token from dashboard_kolones.html.
   The page instead uses the token of the user's current Home Assistant login
   (same origin): from the parent HA frontend when embedded, else from localStorage
   'hassTokens' (refreshed automatically with the refresh token).
   Usage: python3 tools/patch_ha_token.py home_assistant/www/dashboard_kolones.html
"""
import re, sys

BLOCK = r'''// TOKEN: δεν αποθηκεύεται πλέον στο αρχείο (τα αρχεία του /local/ τα βλέπει
// οποιοσδήποτε χωρίς σύνδεση). Η σελίδα χρησιμοποιεί το token της ΤΡΕΧΟΥΣΑΣ
// σύνδεσής σου στο Home Assistant (ίδιο origin):
//   1) αν είναι μέσα σε dashboard (iframe) -> από το Home Assistant που την περιέχει
//   2) αλλιώς -> από το localStorage "hassTokens" (χρειάζεται «Keep me logged in»)
//      και ανανεώνεται αυτόματα πριν λήξει.
function haToken(){
  try{ const h = window.parent !== window && window.parent.document.querySelector('home-assistant');
       const t = h && h.hass && h.hass.auth && h.hass.auth.data && h.hass.auth.data.access_token;
       if(t) return t; }catch(_){}
  try{ return (JSON.parse(localStorage.getItem('hassTokens') || '{}').access_token) || ''; }catch(_){ return ''; }
}
async function haRefreshToken(){
  try{ const t = JSON.parse(localStorage.getItem('hassTokens') || 'null');
       if(!t || !t.refresh_token || (t.expires && t.expires - Date.now() > 5*60*1000)) return;
       const r = await fetch(HA_URL + '/auth/token', { method:'POST', body: new URLSearchParams({
         grant_type:'refresh_token', client_id:t.clientId, refresh_token:t.refresh_token }) });
       if(!r.ok) return; const j = await r.json();
       t.access_token = j.access_token; t.expires = Date.now() + j.expires_in*1000;
       localStorage.setItem('hassTokens', JSON.stringify(t)); }catch(_){}
}
haRefreshToken(); setInterval(haRefreshToken, 60000);'''

def patch(s):
    s, n1 = re.subn(r'const HA_TOKEN = "[^"\n]*";', lambda m: BLOCK, s, count=1)
    s, n2 = re.subn(r'Bearer \$\{HA_TOKEN\}', 'Bearer ${haToken()}', s)
    s, n3 = re.subn(r'access_token:HA_TOKEN\b', 'access_token:haToken()', s)
    assert (n1, n2, n3) == (1, 1, 1), (n1, n2, n3)
    assert 'HA_TOKEN' not in s
    return s

if __name__ == '__main__':
    f = sys.argv[1]; s = open(f, encoding='utf-8').read()
    open(f, 'w', encoding='utf-8').write(patch(s)); print('patched', f)
