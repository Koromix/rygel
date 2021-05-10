var path = require('path').posix;

function isAbsolute(p) {
  return path.isAbsolute(p) || /^[A-Za-z]:\//.test(p);
}

function isExternal(p) {
  return !/^(\.?\.?|[A-Za-z]:)\//.test(p);
}

function absolutify(p, cwd) {
  if(cwd) {
    return path.join(cwd, p);
  } else {
    return './' + p;
  }
}

function forEachInObjectOrMap(object, map, callback) {
  if(object && map) {
    throw Error("Both an Object and a Map were supplied!");
  }
  
  if(map) {
    map.forEach(callback);
  } else if(object) {
    for(var key in object) {
      callback(object[key], key);
    }
  }
  // if neither was supplied, do nothing.
}

module.exports = function rollupPluginHypothetical(options) {
  options = options || {};
  var files0 = options.files;
  var files0AsMap = options.filesMap;
  var allowFallthrough = options.allowFallthrough || false;
  var allowRelativeExternalFallthrough = options.allowRelativeExternalFallthrough || false;
  var allowExternalFallthrough = options.allowExternalFallthrough;
  if(allowExternalFallthrough === undefined) {
    allowExternalFallthrough = true;
  }
  var leaveIdsAlone = options.leaveIdsAlone || false;
  var impliedExtensions = options.impliedExtensions;
  if(impliedExtensions === undefined) {
    impliedExtensions = ['.js', '/'];
  } else {
    impliedExtensions = Array.prototype.slice.call(impliedExtensions);
  }
  var cwd = options.cwd;
  if(cwd !== false) {
    if(cwd === undefined) {
      cwd = process.cwd();
    }
    cwd = unixStylePath(cwd);
  }
  
  var files = new Map();
  if(leaveIdsAlone) {
    forEachInObjectOrMap(files0, files0AsMap, function(contents, f) {
      files.set(f, contents);
    });
  } else {
    forEachInObjectOrMap(files0, files0AsMap, function(contents, f) {
      var unixStyleF = unixStylePath(f);
      var pathIsExternal = isExternal(unixStyleF);
      var p = path.normalize(unixStyleF);
      if(pathIsExternal && !isExternal(p)) {
        throw Error(
          "Supplied external file path \"" +
          unixStyleF +
          "\" normalized to \"" +
          p +
          "\"!"
        );
      }
      if(!isAbsolute(p) && !pathIsExternal) {
        p = absolutify(p, cwd);
      }
      files.set(p, contents);
    });
  }
  
  function basicResolve(importee) {
    if(files.has(importee)) {
      return importee;
    } else if(!allowFallthrough) {
      throw Error(dneMessage(importee));
    }
  }
  
  var resolveId = leaveIdsAlone ? basicResolve : function(importee, importer) {
    importee = unixStylePath(importee);
    
    // the entry file is never external.
    var importeeIsExternal = Boolean(importer) && isExternal(importee);
    
    var importeeIsRelativeToExternal =
      importer &&
      !importeeIsExternal &&
      isExternal(importer) &&
      !isAbsolute(importee);
    
    if(importeeIsExternal) {
      var normalizedImportee = path.normalize(importee);
      if(!isExternal(normalizedImportee)) {
        throw Error(
          "External import \"" +
          importee +
          "\" normalized to \"" +
          normalizedImportee +
          "\"!"
        );
      }
      importee = normalizedImportee;
    } else if(importeeIsRelativeToExternal) {
      var joinedImportee = path.join(path.dirname(importer), importee);
      if(!isExternal(joinedImportee)) {
        throw Error(
          "Import \"" +
          importee +
          "\" relative to external import \"" +
          importer +
          "\" results in \"" +
          joinedImportee +
          "\"!"
        );
      }
      importee = joinedImportee;
    } else {
      if(!isAbsolute(importee) && importer) {
        importee = path.join(path.dirname(importer), importee);
      } else {
        importee = path.normalize(importee);
      }
      if(!isAbsolute(importee)) {
        importee = absolutify(importee, cwd);
      }
    }
    
    if(files.has(importee)) {
      return importee;
    } else if(impliedExtensions) {
      for(var i = 0, len = impliedExtensions.length; i < len; ++i) {
        var extended = importee + impliedExtensions[i];
        if(files.has(extended)) {
          return extended;
        }
      }
    }
    if(importeeIsExternal && !allowExternalFallthrough) {
      throw Error(dneMessage(importee));
    }
    if(importeeIsRelativeToExternal && !allowRelativeExternalFallthrough) {
      throw Error(dneMessage(importee));
    }
    if(!importeeIsExternal && !importeeIsRelativeToExternal && !allowFallthrough) {
      throw Error(dneMessage(importee));
    }
    if(importeeIsRelativeToExternal) {
      // we have to resolve this case specially because Rollup won't
      // treat it as external if we don't.
      // we have to trust that the user has informed Rollup that this import
      // is supposed to be external... ugh.
      return importee;
    }
  };
  
  return {
    resolveId: resolveId,
    load: function(id) {
      if(files.has(id)) {
        return files.get(id);
      } else {
        id = resolveId(id);
        return id && files.get(id);
      }
    }
  };
}

function unixStylePath(p) {
  return p.split('\\').join('/');
}

function dneMessage(id) {
  return "\""+id+"\" does not exist in the hypothetical file system!";
}
