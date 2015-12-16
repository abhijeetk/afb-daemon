(function() {
'use strict';

  var INITIAL_TOKEN=123456789;  // should match with --token=xxxx binder command line

// list all rependencies within the page + controler if needed
angular.module('HomeModule', ['SubmitButton'])

  .controller('HomeController', function ($http, ConfigApp) {
        var scope = this; // I hate JavaScript
        scope.uuid   ="none";
        scope.token  ="none";
        scope.session="none";
        scope.status ="err-no";

        console.log ("Home Controller");
        
        scope.ProcessResponse= function(data, errcode, headers, config) {
            var apiname= 'API'+ data.request.api.replace('-','_');
            scope.status = "err-ok";
            scope.errcode= errcode;
            scope.request  = data.request;
            scope.response = data.response;

            // Make sure we clean everything when Open/Close is called
            if (apiname === "APIcreate" || apiname === "APIreset") {
                scope["APIreset"]='';
                scope["APIcreate"]='';
                scope["APIrefresh"]='';
                scope["APIcheck"]='';
            }
            scope[apiname]="success";
            
            // If we have a new token let's update it
            if (data.request.token) scope.token=data.request.token;
            
            console.log ("OK: "+ JSON.stringify(data));
        };
        
        scope.ProcessError= function(data, errcode, headers, config) {
            var apiname= 'API'+data.request.api.replace('-','_');
            scope.status   = "err-fx";
            scope.errcode  = errcode;
            scope.request  = data.request;
            scope.response = "";
            scope[apiname]="fail";
            
            console.log ("FX: "+ JSON.stringify(data));
        };

        scope.OpenSession = function() {
            console.log ("OpenSession"); 
            var postdata= {/* any json your application may need */};
            var handler = $http.post(ConfigApp.api.token + 'create?token='+INITIAL_TOKEN, postdata);
            
            handler.success(scope.ProcessResponse);
            handler.error(scope.ProcessError);
        };        

        scope.CheckSession = function() {
            console.log ("CloseSession");
            var postdata= {/* any json your application may need */};
            var handler = $http.post(ConfigApp.api.token + 'check?token='+scope.token, postdata);
            
            handler.success(scope.ProcessResponse);
            handler.error(scope.ProcessError);
        };
        
        scope.RefreshSession = function() {
            console.log ("RefreshSession");
            var postdata= {/* any json your application may need */};
            var handler = $http.post(ConfigApp.api.token + 'refresh?token='+scope.token, postdata);
            
            handler.success(scope.ProcessResponse);
            handler.error(scope.ProcessError);
        };
        
        scope.ResetSession = function() {
            console.log ("ResetSession");
            var postdata= {/* any json your application may need */};
            var handler = $http.post(ConfigApp.api.token + 'reset?token='+scope.token, postdata);
            
            handler.success(scope.ProcessResponse);
            handler.error(scope.ProcessError);
        };
        
   });

console.log ("SampleControler Loaded");
})(); 