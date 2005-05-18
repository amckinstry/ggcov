<?php
//
// ggcov - A GTK frontend for exploring gcov coverage data
// Copyright (c) 2005 Greg Banks <gnb@alphalink.com.au>
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
// 
// $Id: cov.php,v 1.1 2005-05-18 13:14:18 gnb Exp $
//

// Status defines
define('COV_COVERED', 0);
define('COV_PARTCOVERED', 1);
define('COV_UNCOVERED', 2);
define('COV_UNINSTRUMENTED', 3);
define('COV_SUPPRESSED', 4);

$cov_rgb = array(
	    array(0,192,0),
	    array(160,160,0),
	    array(192,0,0),
	    array(0,0,0),
	    array(0,0,192));

class cov_callbacks
{
    function fatal($msg)
    {
	echo "<p style=\"color:red;\">$msg</p>\n";
	exit;
    }

    function url_base($base)
    {
	return $base;
    }

    function add_state($query)
    {
	return $query;
    }
}

class cov
{
    var $db_ = false;		    // db resource
    var $cb_;			    // cov_callbacks ref
    var $base_directory_;	    // string
    var $file_index_;		    // array
    var $global_function_list_;	    // array
    var $global_function_index_;    // array
    var $callnode_index_;	    // array
    var $report_index_;		    // array

    function cov($cb)
    {
	if ($cb === null)
	    $cb = new cov_callbacks;
	$this->cb_ = $cb;
    }

    function attach($dir)
    {
	$this->base_directory_ = $dir;
    }

    function detach()
    {
	if ($this->db_ !== false)
	{
	    dba_close($this->db_);
	    $this->db_ = false;
	}
    }

    function fetch($key)
    {
	if ($this->db_ === false)
	{
	    $webdb_file = $this->base_directory_ . '/ggcov.webdb';
	    $this->db_ = dba_open($webdb_file, 'r-', 'db4', 0);
	    if ($this->db_ === false)
		$this->cb_->fatal("Couldn't open $webdb_file");
	}

	$s = dba_fetch($key, $this->db_);
	if ($s === false)
	    $this->cb_->fatal("Couldn't find key \"$key\"");
	return unserialize($s);
    }

    function file_index()
    {
	if ($this->file_index_ === null)
	    $this->file_index_ = $this->fetch('FI');
	return $this->file_index_;
    }

    function global_function_list()
    {
	if ($this->global_function_list_ === null)
	    $this->global_function_list_ = $this->fetch('UL');
	return $this->global_function_list_;
    }

    function global_function_index()
    {
	if ($this->global_function_index_ === null)
	    $this->global_function_index_ = $this->fetch('UI');
	return $this->global_function_index_;
    }

    function callnode_index()
    {
	if ($this->callnode_index_ === null)
	    $this->callnode_index_ = $this->fetch('NI');
	return $this->callnode_index_;
    }

    function report_index()
    {
	if ($this->report_index_ === null)
	    $this->report_index_ = $this->fetch('RI');
	return $this->report_index_;
    }

    function url()
    {
	$args = func_get_args();
	$u = $this->cb_->url_base($args[0]);
	$g = $_GET;
	for ($i = 1 ; $i < count($args) ; $i+=2)
	{
	    $k = $args[$i];
	    $v = $args[$i+1];
	    $g[$k] = $v;
	}
	$g = $this->cb_->add_state($g);
	if (count($g))
	{
	    $c = '?';
	    foreach ($g as $k => $v)
	    {
		$u = $u . $c . $k . '=' . urlencode($v);
		$c = '&';
	    }
	}
	return htmlentities($u);
    }

    function color_by_status($st)
    {
	global $cov_rgb;

	$rgb = $cov_rgb[$st];
	return sprintf('#%02x%02x%02x', $rgb[0], $rgb[1], $rgb[2]);
    }

    function status_by_values($values)
    {
	$numerator = $values[COV_COVERED] + $values[COV_PARTCOVERED];
	$denominator = $numerator + $values[COV_UNCOVERED];

	if ($denominator == 0)
	    $st = COV_UNINSTRUMENTED;
	else if ($numerator == 0)
	    $st = COV_UNCOVERED;
	else if ($values[COV_COVERED] == $denominator)
	    $st = COV_COVERED;
	else
	    $st = COV_PARTCOVERED;

	return $st;
    }

    function status_by_stats($stats)
    {
	return cov::status_by_values($stats[0]);
    }

    // Factory method to create and return a page object
    // best suited for the given URL.
    function create_page($url)
    {
	$url = basename($url);
	$url = preg_replace('/\?.*$/', '', $url);

	switch ($url)
	{
	case 'summary.php':
	    require_once '../lib/summary.php';
	    return new cov_summary_page($this);
	case 'source.php':
	    require_once '../lib/source.php';
	    return new cov_source_page($this);
	case 'files.php':
	    require_once '../lib/files.php';
	    return new cov_files_page($this);
	case 'reports.php':
	    require_once '../lib/reports.php';
	    return new cov_reports_page($this);
	case 'functions.php':
	    require_once '../lib/functions.php';
	    return new cov_functions_page($this);
	case 'calls.php':
	    require_once '../lib/calls.php';
	    return new cov_calls_page($this);
	case 'callbutterfly.php':
	    require_once '../lib/callbutterfly.php';
	    return new cov_callbutterfly_page($this);
	case 'callgraph.php':
	    require_once '../lib/callgraph.php';
	    return new cov_callgraph_page($this);
	default:
	    $this->cb_->fatal("create_page: unknown url");
	}
    }
}

class cov_page
{
    function content_type()
    {
	return 'text/html';
    }

    function render_state_to_form($prefix)
    {
	$state = $this->env_->cb_->add_state(array());
	foreach ($state as $k => $v)
	{
	    $v = htmlentities($v);
	    echo "$prefix<input type=\"hidden\" name=\"$k\" value=\"$v\">\n";
	}
    }
}


?>