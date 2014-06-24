# Copyright (c) 2014, Salesforce.com, Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# - Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# - Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# - Neither the name of Salesforce.com nor the names of its contributors
#   may be used to endorse or promote products derived from this
#   software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
# COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
# USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import functools
import loom.datasets
from nose.tools import assert_true


def assert_found(*filenames):
    for name in filenames:
        assert_true(os.path.exists(name), 'missing file: {}'.format(name))


CLEANUP_ON_ERROR = int(os.environ.get('CLEANUP_ON_ERROR', 1))

TEST_CONFIGS = [
    name
    for name, config in loom.datasets.CONFIGS.iteritems()
    if config['row_count'] <= 100
    if config['feature_count'] <= 100
]


def get_dataset(name):
    return {
        'init': loom.datasets.INIT.format(name),
        'rows': loom.datasets.ROWS.format(name),
        'model': loom.datasets.MODEL.format(name),
        'groups': loom.datasets.GROUPS.format(name),
        'rows_csv': loom.datasets.ROWS_CSV.format(name),
        'schema': loom.datasets.SCHEMA.format(name),
        'encoding': loom.datasets.ENCODING.format(name),
    }


def for_each_dataset(fun):
    @functools.wraps(fun)
    def test_one(dataset):
        files = get_dataset(dataset)
        for path in files.itervalues():
            if not os.path.exists(path):
                raise ValueError(
                    'missing {}, first `python -m loom.datasets init`'.format(
                        path))
        fun(name=dataset, **files)

    @functools.wraps(fun)
    def test_all():
        for dataset in TEST_CONFIGS:
            yield test_one, dataset

    return test_all
