from atlas.typing.typer.base import Typer
from atlas.stats import log_lik_R_S_coverage
MIN_CONF = -99999999
class VariantTyper(Typer):

	def __init__(self, depths, contamination_depths = [], error_rate = 0.05):
		super(VariantTyper, self).__init__(depths, contamination_depths, error_rate)
		self.method = "MAP"

	def type(self, variants):
		if not self.has_contamination():
			self._type_with_minor_model(variants)
		else:
			self._type_without_minor_model(variants)
		return variants

	def _type_with_minor_model(self, variants):
		for variant_name, variants in variants.iteritems():
			for variant in variants:
				self._type_individual(variant, include_minor = True)

		return variants

	def _type_individual(self, variant, include_minor = True):
		hom_ref_likelihood = self._hom_ref_lik(variant)
		hom_alt_likelihood = self._hom_alt_lik(variant)
		if include_minor:
			het_likelihood = self._het_lik(variant)
		else:
			het_likelihood = MIN_CONF
		likelihoods = [hom_ref_likelihood, het_likelihood, hom_alt_likelihood]
		ml = max(likelihoods)
		i = likelihoods.index(ml)
		if i == 0:
			if ml <= MIN_CONF:
				variant.set_genotype("-/-")
			else:
				variant.set_genotype("0/0")
		elif i == 1:
			variant.set_genotype("0/1")
		elif i == 2:
			variant.set_genotype("1/1")

	def _hom_ref_lik(self, variant):
		if variant.reference_percent_coverage < 100:
			return MIN_CONF
		else:			
			hom_ref_likes = []
			## Either alt+cov or alt_covg + contam_covg
			for expected_depth in self.depths:
				hom_ref_likes.append(log_lik_R_S_coverage(variant.reference_median_depth, 
														  variant.alternate_median_depth,
														  expected_depth,
														  error_rate = self.error_rate))			
				for contamination in self.contamination_depths:
					hom_ref_likes.append(log_lik_R_S_coverage(variant.reference_median_depth, 
															  variant.alternate_median_depth,
															  expected_depth + contamination,
															  error_rate = self.error_rate))				
			return max(hom_ref_likes)

	def _hom_alt_lik(self, variant):
		if variant.alternate_percent_coverage < 100:
			return MIN_CONF
		else:	
			hom_alt_liks = []
			## Either alt+cov or alt_covg + contam_covg
			for expected_depth in self.depths:
				hom_alt_liks.append(log_lik_R_S_coverage(variant.alternate_median_depth,
														  variant.reference_median_depth, 
														  expected_depth,
														  error_rate = self.error_rate))			
				for contamination in self.contamination_depths:
					hom_alt_liks.append(log_lik_R_S_coverage(variant.alternate_median_depth,
															  variant.reference_median_depth,   
															  expected_depth + contamination,
															  error_rate = self.error_rate))				
			return max(hom_alt_liks)

	def _het_lik(self, variant):
		if variant.alternate_percent_coverage < 100 or variant.reference_percent_coverage < 100:
			return MIN_CONF
		else:
			return log_lik_R_S_coverage(variant.alternate_median_depth,
									    variant.reference_median_depth,   
									    self.depths[0],
									    error_rate = 0.1)

	def _type_without_minor_model(self, variants):
		raise NotImplementedError("")

	def has_contamination(self):
		return self.contamination_depths or len(self.depths) > 1
