const path = require('path');
const pkg = require('../package.json');

module.exports = {
  assets: [path.join(__dirname, 'assets')],
  project: {
    ios: {
      automaticPodsInstallation: true,
    },
  },
  dependencies: {
    [pkg.name]: {
      root: path.join(__dirname, '..'),
    },
  },
};
