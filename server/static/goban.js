windowSize = function() {
    return [document.documentElement.clientWidth,
            document.documentElement.clientHeight];
};
getVisibleSize = function(selector) {
    var size = windowSize();
    var disp = $(selector);

    var upperOffset = 8,
        leftOffset  = 4;
    var node = disp[0];
    while(node) {
        var top  = node.offsetTop || node.clientTop || 0;
        var left = node.offsetLeft || node.clientLeft || 0;
        upperOffset += top;
        leftOffset  += left;
        node = node.parentNode;
    }

    size[0] -= 2*leftOffset + 1;
    size[1] -= upperOffset + 1;
    return size;
};
onVisibleSizeChange = function(selector,callback,delay) {
    delay = delay || 250;
    var timeout;
    var prevSize = getVisibleSize(selector);
    function check(event) {
        var size = getVisibleSize(selector);
        if(size[0] != prevSize[0] || size[1] != prevSize[1]) {
            console.log((!event ? '[time] ' : '') +
                        'sized changed to ' +
                        JSON.stringify(size));
            callback([size[0],size[1]]);
        }
        prevSize = size;
    }
    function nextTimeout() {
        timeout = setTimeout(function() {
            check(false);
            nextTimeout();
        },delay);
    }

    nextTimeout();

    $(window).bind('resize',check);

    return { cancel: function() {
        clearTimeout(timeout);
    } };
};

function min(x,y) {
    return x < y ? x : y;
}
var boardSize = getVisibleSize("#board");


var board = new WGo.Board(document.getElementById("board"), {
        width: min(boardSize[0],boardSize[1]),
        section: {
                top: -0.5,
                left: -0.5,
                right: -0.5,
                bottom: -0.5,
        }
});
onVisibleSizeChange("#board",function(size) {
    board.setWidth(min(size[0],size[1]));
},100);

var tool = document.getElementById("tool"); // get the &lt;select&gt; element

// WGo.Board.DrawHandler which draws airplanes
var plane = {
        // draw on stone layer
        stone: {
                // draw function is called in context of CanvasRenderingContext2D, so we can paint immediately using this
                draw: function(args, board) {
                        var xr = board.getX(args.x), // get absolute x coordinate of intersection
                                yr = board.getY(args.y), // get absolute y coordinate of intersection
                                sr = board.stoneRadius; // get field radius in px

                        // if there is a black stone, draw white plane
                        if(board.obj_arr[args.x][args.y][0].c == WGo.B) this.strokeStyle = "white";
                        else this.strokeStyle = "black";

                        this.lineWidth = 3;

                        this.beginPath();

                        this.moveTo(xr - sr*0.8, yr);
                        this.lineTo(xr + sr*0.5, yr);
                        this.lineTo(xr + sr*0.8, yr - sr*0.25);
                        this.moveTo(xr - sr*0.4, yr);
                        this.lineTo(xr + sr*0.3, yr - sr*0.6);
                        this.moveTo(xr - sr*0.4, yr);
                        this.lineTo(xr + sr*0.3, yr + sr*0.6);

                        this.stroke();
                }
        },
}

// WGo.Board.DrawHandler which draws coordinates
var coordinates = {
        // draw on grid layer
        grid: {
                draw: function(args, board) {
                        var ch, t, xright, xleft, ytop, ybottom;

                        this.fillStyle = "rgba(0,0,0,0.7)";
                        this.textBaseline="middle";
                        this.textAlign="center";
                        this.font = board.stoneRadius+"px "+(board.font || "");

                        xright = board.getX(-0.75);
                        xleft = board.getX(board.size-0.25);
                        ytop = board.getY(-0.75);
                        ybottom = board.getY(board.size-0.25);

                        for(var i = 0; i < board.size; i++) {
                                ch = i+"A".charCodeAt(0);
                                if(ch >= "I".charCodeAt(0)) ch++;

                                t = board.getY(i);
                                this.fillText(board.size-i, xright, t);
                                this.fillText(board.size-i, xleft, t);

                                t = board.getX(i);
                                this.fillText(String.fromCharCode(ch), t, ytop);
                                this.fillText(String.fromCharCode(ch), t, ybottom);
                        }

                        this.fillStyle = "black";
                }
        }
}

var nextMove = {
    stone: {
        draw: function(args, board) {
            var xr = board.getX(args.x),
                yr = board.getY(args.y),
                sr = board.stoneRadius;

            var radgrad;
            if(args.c == WGo.W) {
                radgrad = this.createRadialGradient(xr-2*sr/5,yr-2*sr/5,sr/3,xr-sr/5,yr-sr/5,8*sr/5);
                radgrad.addColorStop(0, 'rgba(255,255,255,0.5)');
                radgrad.addColorStop(1, 'rgba(102,102,102,0.5)');
            }
            else {
                radgrad = this.createRadialGradient(xr-2*sr/5,yr-2*sr/5,1,xr-sr/5,yr-sr/5,3*sr/5);
                radgrad.addColorStop(0, 'rgba(85,85,85,0.5)');
                radgrad.addColorStop(1, 'rgba(0,0,0,0.5)');
            }

            this.beginPath();
            this.fillStyle = radgrad;
            this.arc(xr-board.ls, yr-board.ls, Math.max(0, sr-board.ls), 0, 2*Math.PI, true);
            this.fill();
        },
    },
    // shadow: WGo.shadow_handler,
}

board.addCustomObject(coordinates);

function makeMove(x,y,black,outline) {
    if(x < 0 || x >= 19 || y < 0 || y >= 19) {
        return null;
    }
    if(board.obj_arr[x][y].length && !board.obj_arr[x][y][0].type) {
        return null;
    }
    var move = { x: x, y: y, c: (black ? WGo.B : WGo.W) };

    if(outline) {
        move.type = nextMove;
    }

    board.addObject(move);

    return move;
}

var moveNum = 0;
var blackCaptives = 0;
var whiteCaptives = 0;
var blacksTurn = true;
var illegalPts = {}
var lastHover = null;
var lastMove = null;

board.addEventListener("mousemove", function(x, y) {
    console.log("hover");
    if(!lastHover || x != lastHover.x || y != lastHover.y) {
        if(lastHover) {
            board.removeObject(lastHover);
            lastHover = null;
        }
        if((blacksTurn ? "B" : "W") == player && !illegalPts[[x,y]]) {
            lastHover = makeMove(x,y,blacksTurn,true);
        }
    }
});

function pass() {
    $.ajax({
        url:'../api/play',
        method:'POST',
        data:{ key: key },
        success: loadNewBoard,
        error: function(hdr,status) {
            var str = ""
            if(hdr.status == 500) {
                str = "server error";
            } else if(hdr.status == 404) {
                str = "game does not exist";
            } else if(hdr.status == 400) {
                str = "move illegal";
            }
            alert("Bad move: " + hdr.status + " (" + str + ") "
                    + hdr.responseText);
        }
    });
}

board.addEventListener("click", function(x, y) {
    if((blacksTurn ? "B" : "W") == player && !illegalPts[[x,y]]) {
        $("#loading").attr("style","");
        $.ajax({
            url:'../api/play',
            method:'POST',
            data:{ key: key, x: x, y: y },
            success: loadNewBoard,
            error: function(hdr,status) {
                var str = ""
                if(hdr.status == 500) {
                    str = "server error";
                } else if(hdr.status == 404) {
                    str = "game does not exist";
                } else if(hdr.status == 400) {
                    str = "move illegal";
                }
                alert("Bad move: " + hdr.status + " (" + str + ") "
                      + hdr.responseText);
            }
        });
    }
});

function loadNewBoard(newBoard) {
    blacksTurn = (newBoard.toMove == "B");
    $("#pass")
        .attr("disabled",
            ((blacksTurn ? "B" : "W") == player) ? null : "");
    blackCaptives = newBoard.blackCaptives;
    whiteCaptives = newBoard.whiteCaptives;
    moveNum = newBoard.moveNum;
    if(lastMove && lastMove.status != "Pass") {
        board.removeObject({ x: lastMove.status[0],
                             y: lastMove.status[1], type: "CR" });
    }
    lastMove = newBoard.lastMove;
    for(var y in newBoard.board) {
        var row = newBoard.board[y];
        for(var x in row) {
            var entry = row[x];
            if(entry == '.') {
                board.removeObject({x:x,y:y});
                board.removeObject({x:x,y:y,type:"SQ"});
            } else if(entry == 'x') {
                board.removeObject({x:x,y:y});
                board.addObject({x:x,y:y,type:"SQ"});
            } else if(entry == 'o') {
                board.addObject({x:x,y:y,c:WGo.B})
            } else if(entry == '*') {
                board.addObject({x:x,y:y,c:WGo.W})
            }
            if(entry != '.') {
                illegalPts[[x,y]] = true;
            }
        }
    }
    if(lastMove && lastMove.status != "Pass") {
        board.addObject({ x: lastMove.status[0], y: lastMove.status[1],
                            type: "CR" });
    }

    $("#bhost").text(newBoard.bhost);
    $("#whost").text(newBoard.whost);
    var status = $("#status")[0];
    var str="";
    if(newBoard.bqueued) {
        str += "B is queued<br/>"
    }
    if(newBoard.wqueued) {
        str += "W is queued<br/>"
    }

    str += "Move <b>" + moveNum + "</b>";
    if(moveNum > 0 && lastMove) {
        str += " (" + (lastMove.color) + " ";
        lastStat = lastMove.status;
        if(lastStat != "Pass") {
            lastStat = "<" + lastStat[0] + "," + lastStat[1] + ">";
        }
        str += lastStat + ")";
    }
    str += ": " + (blacksTurn ? 'B' : 'W') + " to move";
    str += " (B captured: <b>" + whiteCaptives + "</b> / ";
    str += "W captured: <b>" + blackCaptives + "</b>)";
    str += "<br/>"
    if(newBoard['done']) {
        if(!newBoard['scores']) {
            str += "<h2>Game Crashed</h2>";
        } else {
            var b = newBoard['scores'][0]/2.0;
            var w = newBoard['scores'][1]/2.0;
            str += "<h2>Game Over: B " + b + " / W " + w + " (";
            var winner = b > w ? 'B' : 'W';
            str += winner + "+" + Math.abs(b-w) + ")";
            str += "</h2>";
        }
    }
    status.innerHTML = str;

    var illegalArr = newBoard[player == "B" ? 'blackIllegal' :
                                              'whiteIllegal'];
    illegalPts = {}
    for(var x in illegalArr) {
        illegalPts[illegalArr[x]] = true
    }

    console.log(JSON.stringify(newBoard));
    $("#loading").attr("style","display:none");
}

var errorWait = 1000;
var pendingRefresh = false;
function refreshBoard(firstTime) {
    if(pendingRefresh) {
        return;
    }
    pendingRefresh = true;
    $("#loading").attr("style","");
    $.ajax({
        url: firstTime ? '../api/board' : '../api/boardchange',
        method:'POST',
        data:{ key: key, move:moveNum },
        success: function() {
            errorWait = 1000;
            pendingRefresh = false;
            loadNewBoard.apply(this,arguments);
            refreshBoard();
        },
        error: function() {
            setTimeout(function() {
                errorWait *= 2;
                pendingRefresh = false;
                refreshBoard(firstTime);
            },errorWait);
        },
        timeout: 3000
    });
    // $.post("/../api/board", { key:key }, loadNewBoard);
}
setInterval(refreshBoard,3000);

refreshBoard(true);

