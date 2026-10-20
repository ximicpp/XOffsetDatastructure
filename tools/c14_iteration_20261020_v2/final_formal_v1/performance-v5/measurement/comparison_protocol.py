"""Predeclared V5 Q32 / formal64 macrocycle inference; no native I/O.

The two campaigns are fixed before Q. Q publishes six AA tests only and uses the
same V4 binary in both X roles. Formal independently qualifies all AA intervals.
No exclusion, variance-based resizing, optional stopping or frozen import.
Student-t numeric routines are byte-identical to the reviewed V3/V4 source.
"""
from __future__ import annotations
import hashlib
import math
import re

VERSION = 'sc-six-role-cluster-t-v2'
ROLES = ('old_xoffset','new_xoffset','cista','flatbuffers','capnproto','cereal')
COMPETITORS = ROLES[2:]
CONTRASTS = ('old_xoffset',) + COMPETITORS
KINDS = ('qualification','formal')

class ProtocolError(ValueError): pass

def _require(condition,message):
    if not condition: raise ProtocolError(message)

def policy(kind):
    _require(type(kind) is str and kind in KINDS,'invalid campaign kind')
    n = 32 if kind == 'qualification' else 64
    return {
        'protocol':VERSION,'campaign':kind,
        'primary_metric':'complete_native_lifecycle_wall_total_ns',
        'primary_estimand':'geometric_mean_of_within_block_paired_lifecycle_ratios_after_AB_geometric_mean',
        'target_new_over_each_competitor':0.80,
        'blocks':6*n,'roles':6,'replicas_per_role_per_block':2,'children_per_block':12,
        'warmup_blocks':2,'warmup_children':24,'measured_children':72*n,
        'maximum_children':24+72*n,'macrocycles':n,'blocks_per_macrocycle':6,
        'inference_units':n,'degrees_of_freedom':n-1,
        'comparison_family_alpha':0.05,'comparison_contrasts':5,
        'comparison_tails':'five one-sided upper tails',
        'comparisons_published':kind == 'formal',
        'AA_family_alpha':0.05,'AA_roles':6,'AA_tails':'twelve tails for six two-sided intervals',
        'AA_equivalence_factor':1.05,
        'maximum_child_seconds':20,'maximum_window_seconds':2700 if kind == 'qualification' else 5400,
        'cpu_affinity_virtual_cpu':2,'measurement_windows':1,'performance_retries':0,
        'no_outlier_filter':True,'no_optional_stopping':True,'whole_cell_qualification':True,
        'all_blocks_required':True,'log_roundoff_tolerance':1e-12,
        'confidence_model':'independent approximately normal balanced-macrocycle log means; nominal model-conditional coverage',
        'exact_distribution_free_coverage_guaranteed':False,
        'fixed_sample_size_not_adapted_from_Q':True,
        'schedule_seed':'sc-six-role-v5-fixed-schedule-v1',
        'x_role_mapping':'both immutable v4 candidate' if kind == 'qualification' else 'old v4 candidate; new v5 candidate',
    }

def _positive_ns(value, p):
    _require(type(value) is str and len(value)<=20 and re.fullmatch('[1-9][0-9]*',value) is not None,
             'total_ns must be a canonical positive decimal string')
    number=int(value)
    _require(number<2**64 and number<=p['maximum_child_seconds']*10**9,'total_ns exceeds domain/child guard')
    return number

def schedule(kind):
    p=policy(kind);rows=[]
    def block(number,cycle,within,order,jobkind):
        for position,role in enumerate(order):
            ri=ROLES.index(role)
            orientation='AB' if (cycle+within+ri)%2==0 else 'BA'
            for side in orientation:
                rows.append({'id':'j%05d'%len(rows),'kind':jobkind,'role':role,'side':side,
                    'block':number,'macrocycle':cycle,'within_cycle':within,
                    'role_position':position,'order':orientation})
    base=(0,1,5,2,4,3);warm=tuple(ROLES[i] for i in base)
    block(-2,-1,0,warm,'warmup');block(-1,-1,1,tuple(reversed(warm)),'warmup')
    for cycle in range(p['macrocycles']):
        shifts=sorted(range(6),key=lambda shift:hashlib.sha256(
            (p['schedule_seed']+'|%d|%d'%(cycle,shift)).encode('ascii')).digest())
        for within,shift in enumerate(shifts):
            order=tuple(ROLES[(i+shift)%6] for i in base)
            if cycle%2:order=tuple(reversed(order))
            block(cycle*6+within,cycle,within,order,'comparison')
    _require(len(rows)==p['maximum_children'],'internal schedule length')
    return rows

def _beta_fraction(a, b, x):
    """Lentz continued fraction for regularized incomplete beta, bounded."""
    tiny = 1e-300
    qab, qap, qam = a + b, a + 1.0, a - 1.0
    c, d = 1.0, 1.0 - qab * x / qap
    if abs(d) < tiny:
        d = tiny
    d = 1.0 / d
    h = d
    for m in range(1, 501):
        m2 = 2 * m
        aa = m * (b - m) * x / ((qam + m2) * (a + m2))
        d = 1.0 + aa * d
        if abs(d) < tiny:
            d = tiny
        c = 1.0 + aa / c
        if abs(c) < tiny:
            c = tiny
        d = 1.0 / d
        h *= d * c
        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2))
        d = 1.0 + aa * d
        if abs(d) < tiny:
            d = tiny
        c = 1.0 + aa / c
        if abs(c) < tiny:
            c = tiny
        d = 1.0 / d
        delta = d * c
        h *= delta
        if abs(delta - 1.0) <= 2e-14:
            return h
    raise ProtocolError("incomplete beta failed to converge")


def _regularized_beta(x, a, b):
    if x <= 0.0:
        return 0.0
    if x >= 1.0:
        return 1.0
    front = math.exp(math.lgamma(a + b) - math.lgamma(a) - math.lgamma(b)
                     + a * math.log(x) + b * math.log1p(-x))
    if x < (a + 1.0) / (a + b + 2.0):
        return front * _beta_fraction(a, b, x) / a
    return 1.0 - front * _beta_fraction(b, a, 1.0 - x) / b


def student_t_cdf(value, df):
    _require(type(df) is int and df >= 1, "df must be a positive integer")
    _require(type(value) in (int, float) and not math.isnan(value), "invalid t value")
    if value == 0:
        return 0.5
    if math.isinf(value):
        return 0.0 if value < 0 else 1.0
    if abs(value) < math.sqrt(df):
        # This form avoids rounding df/(df+t*t) to exactly1 near the center.
        center = 0.5 * _regularized_beta(value * value / (df + value * value), 0.5, df / 2.0)
        return 0.5 - center if value < 0 else 0.5 + center
    tail = 0.5 * _regularized_beta(df / (df + value * value), df / 2.0, 0.5)
    return tail if value < 0 else 1.0 - tail


def student_t_quantile(probability, df):
    _require(type(df) is int and df >= 1, "df must be a positive integer")
    _require(type(probability) in (int, float) and math.isfinite(probability)
             and 0 < probability < 1, "probability must be strictly between zero and one")
    if probability == 0.5:
        return 0.0
    if probability < 0.5:
        return -student_t_quantile(1.0 - probability, df)
    low, high = 0.0, 1.0
    for _ in range(128):
        if student_t_cdf(high, df) >= probability:
            break
        high *= 2.0
    else:
        raise ProtocolError("t quantile bracketing failed")
    for _ in range(100):
        middle = (low + high) / 2.0
        if student_t_cdf(middle, df) < probability:
            low = middle
        else:
            high = middle
    return (low + high) / 2.0


def _mean(values):
    return math.fsum(values) / len(values)


def _moments(values, p):
    n=p['macrocycles']
    _require(len(values)==n and all(math.isfinite(x) for x in values),'need all finite macrocycle means')
    mean=_mean(values)
    variance=math.fsum((x-mean)**2 for x in values)/(n-1)
    return mean,math.sqrt(variance/n)

def _cycle_means(values,p):
    _require(len(values)==p['blocks'],'need all paired blocks')
    return [_mean(values[c*6:(c+1)*6]) for c in range(p['macrocycles'])]

def analyze(rows,kind):
    p=policy(kind);n=p['macrocycles'];df=n-1;required=p['maximum_children']
    _require(type(rows) is list and len(rows)<=required,'invalid sample list length')
    expected=schedule(kind);totals=[]
    for index,row in enumerate(rows):
        _require(type(row) is dict and set(row)=={'job','total_ns'},'sample fields differ')
        want=expected[index]
        _require(type(row['job']) is dict and set(row['job'])==set(want)
            and all(type(row['job'][k]) is type(v) and row['job'][k]==v for k,v in want.items()),
            'not exact executed schedule prefix at%d'%index)
        totals.append(_positive_ns(row['total_ns'],p))
    _require(sum(totals)<=p['maximum_window_seconds']*10**9,'serialized primary totals exceed window')
    result={'schema_version':2,'protocol':VERSION,'policy':p,
        'successful_children':len(rows),'excluded_warmup_children':min(24,len(rows)),
        'measured_children':max(0,len(rows)-24),'unexecuted_children':required-len(rows),
        'completed_blocks':max(0,(len(rows)-24)//12),'required_blocks':p['blocks'],
        'complete':len(rows)==required,'measurement_qualified':False,'comparison':None,
        'target_achieved':False,'old_new_improvement_established':False,
        'no_outlier_filter':True,'all_measured_rows_retained':True,'failure_reasons':[],
        'old_N3_closed':False,'AA':None,'diagnostic_contrasts':None,
        'confidence_scope':'nominal model-conditional; not distribution-free, not conditional-on-AA-passing coverage'}
    if len(rows)!=required:
        result.update(status='preserved_incomplete',target_status='not_established_incomplete',
            failure_reasons=['incomplete_exact_prefix'])
        return result
    by_block={b:{} for b in range(p['blocks'])}
    for row,ns in zip(rows[24:],totals[24:]):
        j=row['job'];by_block[j['block']].setdefault(j['role'],{})[j['side']]=ns
    raw_aa={r:[] for r in ROLES};levels={r:[] for r in ROLES}
    for b in range(p['blocks']):
        jobs=expected[24+b*12:24+(b+1)*12]
        for role in ROLES:
            v=by_block[b][role]
            first=next(j['side'] for j in jobs if j['role']==role)
            second='B' if first=='A' else 'A'
            raw_aa[role].append(math.log(v[first]/v[second]))
            levels[role].append(.5*(math.log(v['A'])+math.log(v['B'])))
    tol=p['log_roundoff_tolerance'];margin=math.log(1.05)
    t=student_t_quantile(1-.05/12,df);aa={}
    for role in ROLES:
        cycles=_cycle_means(raw_aa[role],p);mean,se=_moments(cycles,p)
        low,high=mean-t*se,mean+t*se
        equivalent=low>=-margin-tol and high<=margin+tol
        aa[role]={'chronological_first_over_second_log_ratios':raw_aa[role],
            'macrocycle_log_means':cycles,'log_estimate':mean,'standard_error':se,'df':df,
            'critical_t':t,'two_sided_ratio_interval':[math.exp(low),math.exp(high)],
            'two_sided_log_interval':[low,high],'equivalence_log_limits':[-margin,margin],
            'simultaneous_family_confidence':.95,'per_role_two_sided_confidence':1-.05/6,
            'equivalence_established':equivalent}
        if not equivalent:result['failure_reasons'].append('AA_equivalence_not_established:'+role)
    qualified=all(a['equivalence_established'] for a in aa.values())
    result.update(status='complete_qualified' if qualified else 'complete_unqualified',
        measurement_qualified=qualified,AA=aa,
        inference={'units':n,'blocks_per_unit':6,'df':df,'independent_native_samples_claimed':False,
            'bootstrap_used':False,'selection_after_outcome':False,
            'model':'independent approximately normal six-block macrocycle log means'},
        retained_measured_job_ids=[r['job']['id'] for r in rows[24:]],
        role_log_level_macrocycles={r:_cycle_means(levels[r],p) for r in ROLES})
    if kind=='qualification':
        result.update(target_status='not_applicable_qualification_AA_only',
            competitor_contrasts_computed=False,qualification_passed=qualified)
        return result
    critical=student_t_quantile(1-.05/5,df);contrasts={}
    for other in CONTRASTS:
        values=[.5*(math.log(by_block[b]['new_xoffset']['A']/by_block[b][other]['A'])+
                    math.log(by_block[b]['new_xoffset']['B']/by_block[b][other]['B'])) for b in range(p['blocks'])]
        cycles=_cycle_means(values,p);mean,se=_moments(cycles,p);upper=mean+critical*se
        threshold=1.0 if other=='old_xoffset' else .8
        contrasts[other]={'numerator':'new_xoffset','denominator':other,
            'block_paired_log_ratios':values,'macrocycle_log_means':cycles,'log_estimate':mean,
            'standard_error':se,'df':df,'critical_t':critical,'ratio_point_estimate':math.exp(mean),
            'one_sided_log_upper':upper,'one_sided_ratio_interval':[0.,math.exp(upper)],
            'one_sided_ratio_upper':math.exp(upper),'per_contrast_one_sided_confidence':.99,
            'simultaneous_family_confidence':.95,'threshold':threshold,
            'point_threshold_pass':mean<=math.log(threshold)+tol,
            'upper_threshold_pass':upper<=math.log(threshold)+tol,
            'first_vs_last_half_log_effect_difference':_mean(cycles[n//2:])-_mean(cycles[:n//2])}
    achieved=qualified and all(contrasts[r]['point_threshold_pass'] and contrasts[r]['upper_threshold_pass'] for r in COMPETITORS)
    if qualified and not achieved:
        result['failure_reasons'].extend('target_upper_not_at_most_0.80:'+r for r in COMPETITORS
            if not(contrasts[r]['point_threshold_pass'] and contrasts[r]['upper_threshold_pass']))
    result.update(diagnostic_contrasts=contrasts,comparison=contrasts if qualified else None,
        target_achieved=achieved,old_new_improvement_established=qualified and
            contrasts['old_xoffset']['one_sided_log_upper'] < -tol,
        target_status='target_established' if achieved else 'target_not_established' if qualified
            else 'not_established_measurement_unqualified')
    return result
