var fs = require('fs'),
    debug = require('debug')('pi-spi'),
    _spi = require("./build/Release/spi_binding");

exports.mode = {
    CPHA: 0x01,
    CPOL: 0x02
};

exports.order = {
    MSB_FIRST: 0,
    LSB_FIRST: 1
};

exports.initialize = function (dev, stub) {
    if (stub) return spiStub;
    var spi = {},
        _fd = fs.openSync(dev, 'r+'),
        _speed = 4e6,
        _mode = 0,
        _order = exports.order.MSB_FIRST;
    
    spi.clockSpeed = function (speed) {
        if (arguments.length < 1) return _speed;
        else _speed = speed;
    };
    spi.dataMode = function (mode) {
        if (arguments.length < 1) return _mode;
        else _mode = mode;
    };
    spi.bitOrder = function (order) {
        if (arguments.length < 1) return _order;
        else _order = order;
    };
    
    
    function _transfer(w,r,cb) {
        _spi.transfer(_fd, _speed, _mode, _order, w, r, cb);
    }
    
    spi.write = function (writebuf, cb) {
        _transfer(writebuf, 0, cb);
    };
    spi.read = function (readcount, cb) {
        _transfer(null, readcount, cb);
    };
    spi.transfer = function (writebuf, readcount, cb) {
        _transfer(writebuf, readcount, cb);
    };
    spi.close = function () {
        fs.close( _fd );
    };
    
    return spi;
};

var spiStub = {
    buf: null,
    clockSpeed: function(speed) {
        debug('[PI-SPI] clock speed ' + speed);
    },
    dataMode: function(mode) {
        debug('[PI-SPI] mode ' + mode);
    },
    bitOrder: function(order) {
        debug('[PI-SPI] bit order ' + order);
    },
    write: function(buf, cb) {
        debug('[PI-SPI] write ' + buf);
        this.buf = buf;
        cb();
    },
    read: function(readcount, cb) {
        debug('[PI-SPI] read ' + readcount);
        cb(null, this.buf);
    }, 
    transfer: function(writebuf, readcount, cb) {
        debug('[PI-SPI] read ' + readcount);
        cb(null, writebuf.slice(0,readcount));
    },
    close: function() {
        debug('[PI-SPI] close ');
    }
}

