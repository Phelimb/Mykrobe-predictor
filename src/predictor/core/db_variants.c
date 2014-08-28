
// cortex_var headers
#include "db_variants.h"
#include "maths.h"


char variant_overflow_warning_printed = 0;










//does not count covg on first or last nodes, as they are bifurcation nodes
//if length==0 or 1  returns 0.
//note I do not want to create an array on the stack - these things can be very long
// so relies on the prealloced array of dBNode* 's passed in
// annoying that can't use templates or something - see below for a similar function with different input
Covg count_reads_on_allele_in_specific_colour(dBNode** allele, int len, int colour,
                                              boolean* too_short)
{
  if(len <= 1)
  {
    *too_short = true;
    return 0;
  }

  // Note: we have to used signed datatypes for this arithmetic
  //       hence using (long) instead of (Covg -- usually uint32_t)

  // note start at node 1, avoid first node
  Covg num_of_reads = db_node_get_coverage_tolerate_null(allele[1], colour);

  int i;

  //note we do not go as far as the final node, which is where the two branches rejoin
  for(i = 2; i < len-1; i++)
  {
    long jump = (long)db_node_get_coverage_tolerate_null(allele[i], colour) -
                (long)db_node_get_coverage_tolerate_null(allele[i-1], colour);

    // we add a little check to ensure that we ignore isolated nodes with higher
    // covg - we count jumps only if they are signifiers of a new read arriving
    // and one node does not a read make
    long diff_between_next_and_prev = -1;

    if(i < len-2)
  	{
      diff_between_next_and_prev
        = (long)db_node_get_coverage_tolerate_null(allele[i+1], colour) -
          (long)db_node_get_coverage_tolerate_null(allele[i-1], colour);
    }

    if(jump > 0 && diff_between_next_and_prev != 0)
    {
      if(COVG_MAX - jump >= num_of_reads)
      {
        num_of_reads += jump;
      }
      else
      {
        num_of_reads = COVG_MAX;

        if(!variant_overflow_warning_printed)
        {
          warn("%s:%i: caught integer overflow"
               "(some kmer coverages may be underestimates)",
               __FILE__, __LINE__);

          variant_overflow_warning_printed = 1;
        }
      }
    }
  }

  return num_of_reads;
}



// WARNING - this is for use when we dissect an allele into subchunks, so here
// we do not want to be ignoring first and last elements (cf above)
Covg count_reads_on_allele_in_specific_colour_given_array_of_cvgs(Covg* covgs,
                                                                  int len,
                                                                  boolean* too_short)
{
  if(len <= 1)
  {
    *too_short = true;
    return 0;
  }

  // Note: we have to used signed datatypes for this arithmetic
  //       hence using (long) instead of (Covg -- usually uint32_t)

  Covg num_of_reads = covgs[0];

  int i;

  for(i = 1; i < len; i++)
  {
    long jump = (long)covgs[i] - covgs[i-1];

    // we add a little check to ensure that we ignore isolated nodes with higher
    // covg - we count jumps only if they are signifiers of a new read arriving
    // and one node does not a read make
    long diff_between_next_and_prev = -1;

    if(i < len-1)
    {
      diff_between_next_and_prev = (long)covgs[i+1] - covgs[i-1];
    }

    if(jump > 0 && diff_between_next_and_prev != 0)
    {
      if(COVG_MAX - jump >= num_of_reads)
      {
        num_of_reads += jump;
      }
      else
      {
        num_of_reads = COVG_MAX;

        if(!variant_overflow_warning_printed)
        {
          warn("%s:%i: caught integer overflow"
               "(some kmer coverages may be underestimates)",
               __FILE__, __LINE__);

          variant_overflow_warning_printed = 1;
        }
      }
    }
  }

  return num_of_reads;
}


//does not count covg on first or last nodes, as they are bifurcation nodes
//if length==0 or 1  returns 0.
Covg count_reads_on_allele_in_specific_func_of_colours(
  dBNode** allele, int len,
  Covg (*sum_of_covgs_in_desired_colours)(const Element *),
  boolean* too_short)
{
  if(len <= 1)
  {
    *too_short = true;
    return 0;
  }

  // Note: we have to used signed datatypes for this arithmetic
  //       hence using (long) instead of (Covg -- usually uint32_t)

  // note start at node 1, avoid first node
  Covg num_of_reads = sum_of_covgs_in_desired_colours(allele[1]);

  int i;

  //note we do not go as far as the final node, which is where the two branches rejoin
  for(i = 2; i < len-1; i++)
  {
    long jump = (long)sum_of_covgs_in_desired_colours(allele[i]) -
                (long)sum_of_covgs_in_desired_colours(allele[i-1]);

    // we add a little check to ensure that we ignore isolated nodes with higher
    // covg - we count jumps only if they are signifiers of a new read arriving
    // and one node does not a read make
    long diff_between_next_and_prev = -1;
    
    if(i < len-2)
    {
      diff_between_next_and_prev
        = (long)sum_of_covgs_in_desired_colours(allele[i+1]) -
          (long)sum_of_covgs_in_desired_colours(allele[i-1]);
    }
      
    if(jump > 0 && diff_between_next_and_prev != 0)
    {
      // Increment number of reads -- avoid overflow
      if(COVG_MAX - jump >= num_of_reads)
      {
        num_of_reads += jump;
      }
      else
      {
        num_of_reads = COVG_MAX;

        if(!variant_overflow_warning_printed)
        {
          warn("%s:%i: caught integer overflow"
               "(some kmer coverages may be underestimates)",
               __FILE__, __LINE__);

          variant_overflow_warning_printed = 1;
        }
      }
    }
  }

  return num_of_reads;
}

//assumes that most of the array is zero, and we want to zero those nodes where covg is
//higher than you expect given proportion of gene covered
int scan_allele_and_remove_repeats_in_covgarray(dBNode** allele, int len, CovgArray* working_ca,
						int colour, 
						int ignore_first, int ignore_last, int expected_covg,
						int* len_populated_in_covg_array)
  
{
  int i;
  if ((len==0)|| (len==1))
    {
      return 0;//ignore first and last nodes
    }
  
  reset_covg_array(working_ca);//TODO - use reset_used_part_of.... as performance improvement. Will do when correctness of method established.
  
  for(i=ignore_first; i < len+1-ignore_last; i++)
    {
      working_ca->covgs[i-ignore_first]
	=db_node_get_coverage_tolerate_null(allele[i], colour);
    }
  
  int array_len =len+1-ignore_first-ignore_last;
  *len_populated_in_covg_array=array_len;
  
  int num_removed=0;
  for(i=0; i < array_len; i++)
    {
      if (working_ca->covgs[i] > 0.5*expected_covg)
	{
	  working_ca->covgs[i] =0;
	  num_removed++;
	}
    }
  
  return num_removed;
}


//robust to start being > end (might traverse an allele backwards)
//if length==0 or 1  returns 0.
//allows you to ignore the first N bases and the last M
Covg median_covg_on_allele_in_specific_colour(dBNode** allele, int len, CovgArray* working_ca,
					      int colour, boolean* too_short, 
					      int ignore_first, int ignore_last,
					      boolean working_ca_pre_initialised)
{

  if ((len==0)|| (len==1))
    {
      *too_short=true;
      return 0;//ignore first and last nodes
    }
 
  if (working_ca_pre_initialised==false)
    {
      reset_covg_array(working_ca);//TODO - use reset_used_part_of.... as performance improvement. Will do when correctness of method established.
      int i;
      
      for(i=ignore_first; i < len+1-ignore_last; i++)
	{
	  working_ca->covgs[i-ignore_first]
	    =db_node_get_coverage_tolerate_null(allele[i], colour);
	}
    }
  int array_len =len+1-ignore_first-ignore_last;
  qsort(working_ca->covgs, array_len, sizeof(Covg), Covg_cmp); 
  working_ca->len=array_len;
  
  Covg median=0;
  int lhs = (array_len - 1) / 2 ;
  int rhs = array_len / 2 ;
  
  if (lhs == rhs)
    {
      median = working_ca->covgs[lhs] ;
    }
  else 
    {
      median = mean_of_covgs(working_ca->covgs[lhs], working_ca->covgs[rhs]);
    }

  return median;
}





Covg median_covg_ignoring_zeroes_on_allele_in_specific_colour(dBNode** allele, int len, CovgArray* working_ca,
							      int colour, boolean* too_short, 
							      int ignore_first, int ignore_last)
{

  if ((len==0)|| (len==1))
    {
      *too_short=true;
      return 0;//ignore first and last nodes
    }
 
  reset_covg_array(working_ca);//TODO - use reset_used_part_of.... as performance improvement. Will do when correctness of method established.
  int i;

  int num_nonzero=0;
  for(i=ignore_first; i < len+1-ignore_last; i++)
    {
      Covg c = db_node_get_coverage_tolerate_null(allele[i], colour);
      if (c>0)
	{
	  working_ca->covgs[num_nonzero] = c;
	  num_nonzero++;
	}
    }

  int array_len =num_nonzero;
  if (array_len==0)
    {
      return 0;
    }

  qsort(working_ca->covgs, array_len, sizeof(Covg), Covg_cmp); 
  working_ca->len=array_len;

  Covg median=0;
  int lhs = (array_len - 1) / 2 ;
  int rhs = array_len / 2 ;
  
  if (lhs == rhs)
    {
      median = working_ca->covgs[lhs] ;
    }
  else 
    {
      median = mean_of_covgs(working_ca->covgs[lhs], working_ca->covgs[rhs]);
    }

  return median;
}


Covg median_of_nonzero_values_of_sorted_covg_array(int len_populated_in_array, CovgArray* working_ca,
						   int colour, 
						   int ignore_first, int ignore_last)
{

  int i;

  int first_nz=0;
  boolean found=false;
  for(i=0; i < len_populated_in_array; i++)
    {
      if ( (i>0) && (working_ca->covgs[i]< working_ca->covgs[i-1]) )
	{
	  die("Qsort has failed to sort in the order I expecte");
	}
      if ( (found==false) && (working_ca->covgs[i]>0) )
	{
	  first_nz=i;
	  found=true;
	}
    }
  //shift left. I know not v efficient
  for(i=first_nz; i < len_populated_in_array; i++)
    {
      working_ca->covgs[i-first_nz]=working_ca->covgs[i];
    }

  int array_len =len_populated_in_array-first_nz;
  if (array_len==0)
    {
      return 0;
    }

  qsort(working_ca->covgs, array_len, sizeof(Covg), Covg_cmp); 
  Covg median=0;
  int lhs = (array_len - 1) / 2 ;
  int rhs = array_len / 2 ;
  
  if (lhs == rhs)
    {
      median = working_ca->covgs[lhs] ;
    }
  else 
    {
      median = mean_of_covgs(working_ca->covgs[lhs], working_ca->covgs[rhs]);
    }

  return median;
}




int longest_gap_on_allele_in_specific_colour(dBNode** allele, 
					   int len, 
					   int colour, 
					   boolean* too_short, 
					   int ignore_first, int ignore_last)
{
  
  if ((len==0)|| (len==1))
    {
      *too_short=true;
      return 0;//ignore first and last nodes
    }
 
  int longest_gap=0;
  int curr_gap=0;
  Covg prev=1;
  int i;
  for(i=ignore_first; i < len+1-ignore_last; i++)
    {
      Covg c = db_node_get_coverage_tolerate_null(allele[i], colour);

      if (c==0) 
	{
	  curr_gap++;
	}
      else if (prev==0)//end of a gap
	{
	  if (curr_gap>longest_gap)
	    {
	      longest_gap=curr_gap;
	    }
	  curr_gap=0;
	}
      prev=c;
    }
  if (curr_gap>longest_gap)
    {
      longest_gap=curr_gap;
    }
  return longest_gap;
}




//robust to start being > end (might traverse an allele backwards)
//if length==0 or 1  returns 0.
Covg min_covg_on_allele_in_specific_colour(dBNode** allele, int len, int colour, boolean* too_short,
					   int ignore_first, int ignore_last)
{

  if ((len==0)|| (len==1))
    {
      *too_short=true;
      return 0;//ignore first and last nodes
    }
 
  int i;

  Covg min_covg = COVG_MAX;
  for(i=ignore_first; i < len+1-ignore_last; i++)
    {
      if (allele[i]!=NULL)
	{
	  Covg c=db_node_get_coverage_tolerate_null(allele[i], colour);
	  if (c<min_covg)
	    {
	      min_covg = c;
	    }
	}
      else
	{
	  min_covg=0;
	}
      
    }

  return min_covg;
}


int percent_nonzero_on_allele_in_specific_colour(dBNode** allele, int len, int colour, boolean* too_short,
						 int ignore_first, int ignore_last)
{

  if ((len==0)|| (len==1))
    {
      *too_short=true;
      return 0;//ignore first and last nodes
    }
 
  int i;

  int num_nonzero=0;

  for(i=ignore_first; i < len+1-ignore_last; i++)
    {
      if (allele[i]!=NULL)
	{
	  Covg c=db_node_get_coverage(allele[i], colour);
	  if (c>0)
	    {
	      num_nonzero++;
	    }
	}
    }
  return num_nonzero*100/(len+1-ignore_first-ignore_last);
}


