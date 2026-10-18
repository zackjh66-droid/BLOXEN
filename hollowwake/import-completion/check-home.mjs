import playwright from '/opt/codex/runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright/index.js';
const {chromium}=playwright;
import assert from 'node:assert/strict';import {writeFile} from 'node:fs/promises';
const browser=await chromium.launch({headless:true,executablePath:'/usr/bin/chromium',args:['--no-sandbox','--disable-dev-shm-usage']});const results=[];
try{
 for(const viewport of [{width:1440,height:1000},{width:390,height:844}]){
  const page=await browser.newPage({viewport});const errors=[];page.on('pageerror',e=>errors.push(e.message));await page.goto('http://127.0.0.1:8793/',{waitUntil:'networkidle'});await page.locator('#home-journey').waitFor({state:'visible'});await page.locator('#journey-account').scrollIntoViewIfNeeded();await page.waitForTimeout(650);
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'Horizontal overflow');assert.equal(await page.locator('#journey-account').evaluate(e=>getComputedStyle(e.querySelector('.chapter-copy')).opacity),'1');assert.equal(await page.locator('#reading-progress-fill').evaluate(e=>getComputedStyle(e).transform).then(t=>t==='matrix(0, 0, 0, 1, 0, 0)'),false);
  for(const id of ['journey-start','journey-account','journey-client']){await page.locator('#'+id).scrollIntoViewIfNeeded();await page.waitForTimeout(550);}
  await page.evaluate(()=>scrollTo({top:0,behavior:'instant'}));await page.waitForTimeout(150);await page.screenshot({path:`/workspace/hollowwake/import-completion/evidence/home-hero-${viewport.width}.png`});
  await page.screenshot({path:`/workspace/hollowwake/import-completion/evidence/home-${viewport.width}.png`,fullPage:true});assert.deepEqual(errors,[]);
  await page.emulateMedia({reducedMotion:'reduce'});assert.equal(await page.locator('#journey-client .chapter-copy').evaluate(e=>getComputedStyle(e).opacity),'1');assert.equal(await page.locator('#journey-client .chapter-copy').evaluate(e=>getComputedStyle(e).transitionDuration),'0s');
  await page.goto('http://127.0.0.1:8793/link',{waitUntil:'networkidle'});assert.equal(await page.locator('#home-journey').isVisible(),false);assert.equal(await page.locator('#auth-form').isVisible(),true);assert.equal(await page.locator('.journey-nav a').first().isVisible(),false);
  results.push({viewport,homeChapters:true,noHorizontalOverflow:true,progress:true,reducedMotion:true,linkFormPreserved:true,pageErrors:errors});await page.close();
 }
 console.log(JSON.stringify(results,null,2));await writeFile('/workspace/hollowwake/import-completion/evidence/home-browser-results.json',JSON.stringify({scope:'Local offline candidate; disposable DB, not live deployment',results},null,2));
}finally{await browser.close()}
