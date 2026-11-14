// Run from the repository root: node tests/arima_wasm_test.cjs
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const {pathToFileURL, fileURLToPath} = require('node:url');
const assert = require('node:assert/strict');
(async () => {
 const factory = require('../Binding/levy_wasm.js');
 const context = vm.createContext({ URL, console,
 document: {currentScript: {src:pathToFileURL(path.resolve('Binding/levy-wasm.js')).href}},
 createLevyModule: () => factory({wasmBinary:fs.readFileSync('Binding/levy_wasm.wasm')}),
 fetch: async url => ({ok:true,text:async()=>fs.readFileSync(fileURLToPath(url),'utf8')})
 });
 vm.runInContext(fs.readFileSync('Binding/levy-wasm.js','utf8')+'\nglobalThis.api = LevyWasm;', context);
 for (const [file, column] of [['processed/brent_daily.csv','price'], ['raw/usd_pkr_daily.csv','usd_pkr']]) {
 const lines=fs.readFileSync('data/'+file,'utf8').trim().split(/\r?\n/);
 const headers=lines.shift().split(',');
 const prices=lines.map(line=>Number(line.split(',')[headers.indexOf(column)])).filter(x=>x>0);
 for (const horizon of [12,24]) {
 const result = await context.api.arimaReport(prices,horizon);
 assert(result.ok,result.error);
 for (const key of ['mean','next','mape','rmse','mae']) assert(Number.isFinite(result[key]),key);
 assert(result.p>=1 && result.p<=10); assert.equal(result.d,1);
 assert.equal(result.testCount,horizon); assert.equal(result.trainCount,prices.length-horizon);
 assert(Math.abs(result.mean-await context.api.arimaMean(prices,horizon))<1e-8);
 assert(result.rmse>=result.mae-1e-8);
 console.log(file,horizon,JSON.stringify(result));
 }
 }
 const constant=await context.api.arimaReport(Array(320).fill(100),12);
 assert(constant.ok); assert(constant.rmse<1e-8); assert(constant.mape<1e-8);
 assert(!(await context.api.arimaReport([100,101],12)).ok);
 assert((await context.api.live(75)).ok,'Levy recommendation regression');
 assert((await context.api.report()).ok,'Tree report regression');
 console.log('WASM bridge checks passed');
})().catch(error=>{console.error(error);process.exitCode=1});
